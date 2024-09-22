#!/bin/bash

case "$(uname -s)" in
    Linux*) 
        ./linux_build.sh
        ;;
    Darwin*)
        ./macos_build.sh
        ;;
    *)
        echo "Unsupported platform: $(uname -s)"
        exit 1
        ;;
esac

./copy_scripts.sh
