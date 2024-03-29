# Aurman
A simple and fast command line utility to help manage your AUR applications

## Installation

### Dependencies
* `git`
* `curl`
* `jansson`

Install dependencies via
```bash
sudo pacman -S git curl jansson
```

Simply clone the repository
```bash
git clone https://github.com/pranavsetpal/aurman.git
cd aurman
```
And run the installation script
```bash
sudo sh install.sh
```
\
The script simply compiles it using gcc and moves the executable to `/usr/local/bin/`. The `.aurman` directory is created beforehand for the `--all` option to work as expected.
```bash
gcc aurman.c -lm -lcurl -ljansson -o aurman
mv -f ./aurman /usr/local/bin/
mkdir $HOME/.aurman/
```
