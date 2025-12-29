#!/usr/bin/env zsh
set -eu

declare -a prog
declare -i all=0
declare -i print_fix=0
declare -i fix=0
declare -i confirm=0

#
# Parse command line
#

if (( $# > 0 ))
then
    case $1 in
        -h) echo "Show image when tests fails."
            echo
            echo 'Usage:'
            echo "  bjam |& $0 [-a | -i | -f | -F] [prog prog_params...]"
            echo
            echo 'Arguments:'
            echo
            echo ' -a  Show all images.'
            echo ' -i  Show images on test failure and confirm to '
            echo '     replace the original image with the output image.'
            echo ' -f  Generate a copy/pastable fix.'
            echo ' -F  Execute the fix.'
            echo
            echo 'Environment variables:'
            echo '  - IMAGE_VIEW: default image command line.'
            exit 1
            ;;
        -a) shift; all=1 ;;
        -i) shift; confirm=1 ;;
        -f) shift; fix=1 ; print_fix=1 ;;
        -F) shift; fix=1 ; print_fix=0 ;;
        #*) prog
    esac

    prog=($@)
fi

#
# Init prog when empty
#

if (( ! $#prog ))
then
    if [[ -n ${IMAGE_VIEW:-} ]]
    then
        prog=($=IMAGE_VIEW)
    elif (( fix ))
    then
        prog=(cp)
    else
        prog=(imv -snone -b'444444')
    fi
fi

#
# Force printing program with print_fix=1
#

if (( print_fix ))
then
    prog=(echo $prog:q)
fi

#
# Main
#

declare -a imgs
while IFS='' read -r line
do
    if (( ! print_fix ))
    then
        echo -E $line
    fi

    if [[ $line = '  Image path: '* ]]
    then
        #
        # Parse image paths
        #

        IFS='' read -r line2 || break
        IFS='' read -r line3 || break

        if (( ! print_fix ))
        then
            echo -E $line2
            echo -E $line3
        fi

        read _ _ img_out  <<<$line
        read _ _ img_diff <<<$line2
        read _ _ img_ref  <<<$line3

        #
        # Append paths
        #
        if (( fix ))
        then
            imgs+=($img_out $img_ref)
        else
            imgs+=($img_out)

            if [[ $img_diff = [./]* ]]
            then
                imgs+=($img_diff)
            fi

            if [[ $img_ref = [./]* && -f $img_ref ]]
            then
                imgs+=($img_ref)
            fi
        fi

        #
        # Show images
        #
        if (( ! all ))
        then
            $prog $imgs
            imgs=()
        fi

        #
        # Ask image replacement
        #
        if (( confirm ))
        then
            echo "\e[33m/bin/cp $img_ref:q $img_out:q\e[m\nReplace ? [y/n]" >&2
            if read -q
            then
                echo
                cp $img_out $img_ref
            else
                echo
            fi
        fi
    fi
done

if (( all && $#imgs ))
then
    $prog $imgs
fi
