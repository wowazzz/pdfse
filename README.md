# Pdf Spots Extractor


## Prerequisites

 - PoDoFo


Install PoDoFo from Ubuntu Packages (0.9.5):

	sudo apt update
	sudo apt install libpodofo-dev libpodofo-utils libpodofo0.9.5


or install PoDoFo from source (current version: 0.9.6):

	sudo apt update
	sudo apt install zlib1g-dev libfreetype6-dev fontconfig libfontconfig1-dev libjpeg-dev libcrypto++-dev libidn11-dev libtiff-dev libunistring-dev libcppunit-dev libcrypto++6 lua5.1 lua5.1-dev
	cd ~
	mkdir podofo-src
	mkdir podofo-build
	svn checkout https://svn.code.sf.net/p/podofo/code/podofo/trunk/ podofo-src
	cd podofo-build
	cmake -G "Unix Makefiles" ../podofo-src
	make
	sudo make install
	
	
### Installing Pdf Spots Extractor

    git clone https://github.com/wowazzz/pdfse.git pdfse
	cd pdfse
	./make_pdfse


### Running parameters

	Usage:
	 pdfse input_file.pdf [-options] Spot1 [ ... SpotN ]

	Options:
	  -d, --debug    enable Debug mode.


### Example

	./pdfse ./test/sample.pdf RedSpot GoldSpot
	
	
It will create files sample.RedSpot.pdf, sample.GoldSpot.pdf and sample.remaining.pdf files in /test directory.


