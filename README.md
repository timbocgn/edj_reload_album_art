# edj_reload_album_art


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
