#
# DirectHW - Kernel extension to pass through IO commands to user space
#
# Copyright © 2008-2010 coresystems GmbH <info@coresystems.de>
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

all: main libs

is11   := $(shell bc <<< "$${OSTYPE\#darwin} >= 20")
is10_6 := $(shell bc <<< "$${OSTYPE\#darwin} >= 10 && $${OSTYPE\#darwin} < 17") # change 17 (10.13) to lowest version that can't build the 10.6 project
is10_5 := $(shell bc <<< "$${OSTYPE\#darwin} >= 9  && $${OSTYPE\#darwin} < 10")
is10_4 := $(shell bc <<< "                            $${OSTYPE\#darwin} <  9")

# latestsdk := $(shell xcodebuild -showsdks | sort -r | sed -nE '/.*(-sdk macosx.*)/ { s//\1/;p;q; }')

proj := DirectHW.xcodeproj
build := build
extensions := /Library/Extensions
debugsym :=
sdk := -sdk macosx
deploy := MACOSX_DEPLOYMENT_TARGET=10.6

ifeq ($(is10_6), 1)
	proj := DirectHW10.6.xcodeproj
	build := build/build10.6
	sdk := 
	deploy := 
endif
ifeq ($(is10_5), 1)
	proj := DirectHW10.5.xcodeproj
	build := build/build10.5
	extensions := /System/Library/Extensions
	debugsym := -g
	sdk := 
	deploy := 
endif
ifeq ($(is10_4), 1)
	proj := DirectHW10.4.xcodeproj
	build := build/build10.4
	extensions := /System/Library/Extensions
	sdk := 
	deploy := 
endif

main:
	xcodebuild -alltargets -project $(proj) $(sdk) $(deploy)

libs: DirectHW.c DirectHW.h
	$(CC) DirectHW.c -dynamiclib -framework IOKit -o $(build)/Release/libDirectHW.dylib $(debugsym)
	#$(CC) -static -c DirectHW.c -o $(build)/Release/libDirectHW.a

install:
	sudo mkdir -p /usr/local/lib
	[[ -e $(extensions)/DirectHW.kext ]] && sudo rm -R $(extensions)/DirectHW.kext || :
	sudo cp -R $(build)/Release/DirectHW.kext $(extensions)/
	[[ -e /Library/Frameworks/DirectHW.framework ]] && sudo rm -R /Library/Frameworks/DirectHW.framework || :
	sudo cp -R $(build)/Release/DirectHW.framework /Library/Frameworks/
	sudo cp $(build)/Release/libDirectHW.a /usr/local/lib/
	sudo cp $(build)/Release/libDirectHW.dylib /usr/local/lib/
	sudo chmod -R 755 $(extensions)/DirectHW.kext
	sudo chmod -R 755 /Library/Frameworks/DirectHW.framework
	sudo chmod 644 /usr/local/lib/libDirectHW.a
	sudo chmod 644 /usr/local/lib/libDirectHW.dylib
	sudo chown -R root:wheel $(extensions)/DirectHW.kext
	sudo chown -R root:wheel /Library/Frameworks/DirectHW.framework
	sudo kextunload -v $(extensions)/DirectHW.kext || :
	sudo kextload -v $(extensions)/DirectHW.kext
	sudo touch $(extensions) || :
	command -v kmutil > /dev/null && sudo kmutil install --volume-root / --check-rebuild || :

clean:
	rm -rf build
