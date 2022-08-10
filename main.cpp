/*
    ------------------------------------------------------------------------------------------------------

    edj_reload_album_art command line utility

    This small utility reload all missing images from the actual songs files into the engine database.
    It parses all source files, converts to the right format and directly adds to the Engine DJ
    track database.

    This utility is intended to be used in conjuction to "Lexicon" (https://www.lexicondj.com)

    Use it on the USB devices prepared for your Denon gear:

    Usage: edj_reload_album_art <path_to_usb_device>

    Why?

    Lexicon does an amazing job generating Engine DJ databases, but does not add album art to it. 
    This demands in the end a quite slow workflow, which can be improved by this utility.

    Workflow

    - Assumption is you have all your tracks in Lexicon and want to prep a USB device for your gig
    - Export all the playlist you want to use on the USB drive using Lexicon functions
    - After this is finished, just invoke edj_reload_album_art /Volume/USB-Drive 

    This will then detect all files w/o album art, parse the song files, extract the image, 
    convert it and store it in the database. After this, the Denon gear shows all images again.

    Building (for dev folks)

    - the build system is *depending* on "homebrew" for macos. It does not work on Windows / Linux
    - you will have to install imagemagick 7 as library using "brew install imagemagick"
    - download taglib v1.12 and expand the archive onto a taglib-1.12 directory
    - Just use cmake . and then make. 
        
    Acknowledgements

    - TagLib is used for track file parsing
    - sqlite for accessing the target database
    - ImageMagick for the image processing 

    ------------------------------------------------------------------------------------------------------
    Copyright (c) 2022 Tim Hagemann / way2.net Services

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

    ------------------------------------------------------------------------------------------------------
*/
#include <iostream>
#include <fstream>
#include <map>
#include <string>
#include <unistd.h>
#include <vector>
#include <sstream>

using namespace std;

#include "sqlite3.h"

// ---- Magick++ includes

#include <Magick++.h>
using namespace Magick;

// ---- taglib includes

#include "tag.h"
#include "fileref.h"

#include <id3v2tag.h>
#include <mpegfile.h>
#include <id3v2frame.h>
#include <id3v2header.h>
#include <attachedpictureframe.h>

#include <mp4file.h>
#include <mp4tag.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void *HandleM4A(const char *f_filename, int &f_Size)
{
    TagLib::MP4::File f(f_filename);
    if (!f.isValid()) return NULL;

    TagLib::MP4::Tag *tag = static_cast<TagLib::MP4::Tag *>(f.tag());
    if (!tag) return NULL;

    /*
        const TagLib::MP4::ItemMap l_im = tag->itemMap();

        for (TagLib::MP4::ItemMap::ConstIterator it = l_im.begin(); it != l_im.end(); ++it)
        {
            cout << it->first << endl;
    
        }
    */

    TagLib::MP4::Item coverItem = tag->item("covr");
    if (!coverItem.isValid()) 
    {    
       return NULL;
    }
 
    TagLib::MP4::CoverArtList coverArtList = coverItem.toCoverArtList();
    TagLib::MP4::CoverArt coverArt = coverArtList.front();

    f_Size = coverArt.data().size();

    void *SrcImage = malloc ( f_Size ) ;
    memcpy ( SrcImage, coverArt.data().data(), f_Size ) ;

    return SrcImage;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void *HandleMP3(const char *f_filename, int &f_Size)
{
    void *SrcImage = NULL;

    TagLib::MPEG::File mpegFile(f_filename);
    if (!mpegFile.isValid()) return NULL;

    TagLib::ID3v2::Tag *id3v2tag = mpegFile.ID3v2Tag();
    TagLib::ID3v2::FrameList Frame;
    TagLib::ID3v2::AttachedPictureFrame *PicFrame;

    if ( id3v2tag )
    {
        //printf("id3v2 tag found\n");

        // picture frame
        Frame = id3v2tag->frameListMap()[ "APIC" ] ;

        if (!Frame.isEmpty() )
        {
            //printf("frame tag found\n");

            // find cover art
            for(TagLib::ID3v2::FrameList::ConstIterator it = Frame.begin(); it != Frame.end(); ++it)
            {
                PicFrame = (TagLib::ID3v2::AttachedPictureFrame *)(*it) ;
                if ( PicFrame->type() == TagLib::ID3v2::AttachedPictureFrame::FrontCover)
                {

                    //printf("front cover found\n");

                    // extract image (in it's compressed form)
                    f_Size = PicFrame->picture().size() ;
                    SrcImage = malloc ( f_Size ) ;
            
                    memcpy ( SrcImage, PicFrame->picture().data(), f_Size ) ;

                }
            }
        }
    }

    return SrcImage;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void AddImage(sqlite3 *db,int f_track_id, Blob &f_blob)
{
    sqlite3_stmt *stmt = NULL;
 
    // ---- insert blob into albumart table

    int rc = sqlite3_prepare_v2(db,"INSERT INTO AlbumArt(albumArt) VALUES(?)",-1, &stmt, NULL);
    if (rc != SQLITE_OK) 
    {
        cerr << "ERROR: prepare insert album art blob failed: " << sqlite3_errmsg(db) << endl;
        return;
    } 

    // SQLITE_STATIC because the statement is finalized
    // before the buffer is freed:

    rc = sqlite3_bind_blob(stmt, 1, f_blob.data(), f_blob.length(), SQLITE_STATIC);
    if (rc != SQLITE_OK) 
    {
        cerr << "ERROR: bind insert album art blob failed: " << sqlite3_errmsg(db) << endl;
        return;
    } 

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        cerr << "ERROR: exec insert album art blob failed: " << sqlite3_errmsg(db) << endl;
        return;
    }
    
    sqlite3_int64 l_aaid = sqlite3_last_insert_rowid(db);

    sqlite3_finalize(stmt);

    // ---- insert foreing key into track table

    rc = sqlite3_prepare_v2(db,"UPDATE Track SET albumArtId = ? WHERE id = ?",-1, &stmt, NULL);
    if (rc != SQLITE_OK) 
    {
        cerr << "ERROR: prepare insert album art id failed: " << sqlite3_errmsg(db) << endl;
        return;
    } 

    // SQLITE_STATIC because the statement is finalized
    // before the buffer is freed:

    rc = sqlite3_bind_int64(stmt,1,l_aaid);
    if (rc != SQLITE_OK) 
    {
        cerr << "ERROR: bind insert album art id failed (aaid): " << sqlite3_errmsg(db) << endl;
        return;
    } 

    rc = sqlite3_bind_int(stmt,2,f_track_id);
    if (rc != SQLITE_OK) 
    {
        cerr << "ERROR: bind insert album art id failed (trackid): " << sqlite3_errmsg(db) << endl;
        return;
    } 

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE)
    {
        cerr << "ERROR: exec insert album art id failed: " << sqlite3_errmsg(db) << endl;
        return;
    }
 
    cout << "inserted " << sqlite3_last_insert_rowid(db) << endl;

 
    sqlite3_finalize(stmt);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
    InitializeMagick(NULL);

    std::cout << "edj_reload_album_art v1.0 - reload engine DJ album art from music file" << std::endl;
    std::cout << "----------------------------------------------------------------------" << std::endl << std::endl;

    if (argc != 2)
    {
        std::cout << "Usage: edj_reload_album_art <path_to_export_media>" << std::endl << std::endl;
        return 255;
    }

    // --- calculate filename to engine database

    std::string l_filename_denon = std::string(argv[1]) + "/Engine Library/Database2/m.db";

    // --- open Denon DJ SQLite db
    
    std::cout << "Opening Denon database file '" << l_filename_denon << "'" << std::endl << endl;

    sqlite3 *l_sql_db;

    int l_rc = sqlite3_open_v2(l_filename_denon.c_str(), &l_sql_db,SQLITE_OPEN_READWRITE,NULL);
    if (l_rc)
    {
        std::cerr << "Error opening database: " << sqlite3_errmsg(l_sql_db) << std::endl;
        sqlite3_close(l_sql_db);

        return 255;
    }

    // --- open the rekordbox database file

    try
    {
        // --- create SQL statement, that finds all tracks without album art

        sqlite3_stmt* l_stmt;

        std::string l_sql = "select track.id,path,filename from Track,AlbumArt where Track.albumArtId = albumArt.id and albumArt.albumArt is NULL";

        if (sqlite3_prepare_v2(l_sql_db, l_sql.c_str(), -1, &l_stmt, NULL) != SQLITE_OK) 
        {
            printf("ERROR: while compiling sql: %s\n", sqlite3_errmsg(l_sql_db));
            sqlite3_close(l_sql_db);
            sqlite3_finalize(l_stmt);
            return -1;
        }

        // --- some counters

        int l_added_images = 0;
        int l_warnings = 0;

        // --- execute sql statement, and while there are rows returned, process
        
        int ret_code = 0;
        while ((ret_code = sqlite3_step(l_stmt)) == SQLITE_ROW) 
        {
            //printf("TEST: ID = %d\n", sqlite3_column_int(l_stmt, 0));

            // ---- create full qualified filename of audio file

            stringstream l_ss;

            l_ss << string(argv[1]) << string("/Engine Library/") << string((char*)sqlite3_column_text(l_stmt, 1));

            string l_fullpath = l_ss.str();

            // --- some feedback to the user

            int l_track_id = sqlite3_column_int(l_stmt, 0);

            cout << "Processing ID " << l_track_id << ": " << (char*)sqlite3_column_text(l_stmt, 2) << endl;
            
            // ---- now for all file types...
            
            TagLib::String fileType = l_fullpath.substr(l_fullpath.size() - 3);
            fileType = fileType.upper();

            void *l_SrcImage = NULL;
            int   l_Size;

            if (fileType == "MP3")
            {
                l_SrcImage = HandleMP3(l_fullpath.c_str(),l_Size);
            }
            else if (fileType == "M4A")
            {
                l_SrcImage = HandleM4A(l_fullpath.c_str(),l_Size);
            }
            else
            {
                cout << "       WARNING: unsupported file type " << fileType << endl;
            }

            // ---- process the image, if there is one

            if (l_SrcImage)
            {
                // --- read image from blob, resize and write to blob

                Blob blob( l_SrcImage, l_Size ); 
                Image image( blob );

                image.resize(Geometry(256,256));

                Blob l_new_image_blob; 
                image.magick( "PNG" ); 
                image.write( &l_new_image_blob );

                // --- add image to the database

                AddImage(l_sql_db,l_track_id,l_new_image_blob);

                free( l_SrcImage );
                l_added_images++;
            }
            else
            {
                cout << "       WARNING: no image found." << endl;
                l_warnings++;
            }


        
        }

        // --- check if something bad happened what led to exiting the loop
        
        if(ret_code != SQLITE_DONE) 
        {
            // --- this error handling could be done better, but it works
            cerr << "ERROR: while performing sql:" << sqlite3_errmsg(l_sql_db) << ". Return code " << ret_code << endl;
            return -1;
        }

        // --- release resources
        
        sqlite3_finalize(l_stmt);

        std::cout << "Added " << l_added_images << " images." << endl;
        std::cout << "Found " << l_warnings << " files with an issue" << endl;
    
        // --- cleanup

        sqlite3_close(l_sql_db);

    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return -1;
    }
    
    

    return 0;
}


