gcc aurman.c -lm -lcurl -ljansson -o aurman
mv -f ./aurman /usr/local/bin/
mkdir $HOME/.aurman/
