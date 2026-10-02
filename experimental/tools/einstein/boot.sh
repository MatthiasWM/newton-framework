#!/bin/zsh
# boot.sh <address> [seconds]: build the ROM with 16 bytes put in before the
# symbol at <address>, make an Einstein image of it (romimage.py --einstein),
# boot it, print the milestones it reached
E=${0:a:h:h:h}; T=$E/tools
AT=$1; D=$E/build/einstein/pad_$AT
python3 $T/romasm.py $E/build/rom $D/src --pad $AT:16 >/dev/null || exit 1
python3 -c "
import sys; sys.path.insert(0,'$T')
from romlink import build
sys.exit(0 if build('$E/bin','$D/src','$D/obj') else 1)" >/dev/null || exit 1
python3 $T/romimage.py --aif $D/obj/rom.aif --rom $E/build/rom -o $D/rom.image --einstein >/dev/null || exit 1
python3 ${0:a:h}/watch.py $D/obj/symbols.txt $D/rom.image >/dev/null
${0:a:h}/run.sh $D/rom.image ${2:-40} pad_$AT | head -1
rm -rf $D/src $D/obj
