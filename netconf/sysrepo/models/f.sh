#!/bin/bash

DIRREGEX="$1"
REGEX="$2"

#for file in $(find . -regex "^.*@.*");
for file in $(find . -regex $DIRREGEX);
do
  echo "$file"
  #mv "$file" "$(echo $file | sed "s/\(@.*\).yang//")";
  mv "$file" "$(echo $file | sed "s/$REGEX//")";
done
