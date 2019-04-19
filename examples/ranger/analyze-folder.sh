
#!/usr/bin/env sh

set -e

files=$1

if [ -z $files ]
then
    files=`pwd`
fi

if [ -d $files ]
then
    files=$files/*
fi

for f in $files
do
    # echo "Starting analysis for $f"
    python3 analyzer.py $f
    # echo "Analysis done..."
done

echo "All files have been analised"
echo "Done..."

