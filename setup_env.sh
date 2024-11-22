echo "[+] Sourcing the setenvs"
source $PWD/../../setenv && \
source $PWD/../inet4.4/setenv && \

echo "[+] Doing makefile generation" && \
OLDPWD=$PWD && \
cd $PWD/../.. && \
make makefiles && \
cd $OLDPWD && \

echo "[+] Setting functions" && \
CORES=$(nproc) || \
exit 1

function rebuild_inet() {
	OLDPWD=$PWD && \
	cd $PWD/../inet4.4 && \
	make makefiles && \
	make clean && \
	make MODE=release -j${CORES} && \
	make MODE=debug -j${CORES} && \
	cd $OLDPWD
}

function rebuild_flora() {
	make makefiles && \
	make clean && \
	make MODE=release -j${CORES} && \
	make MODE=debug -j${CORES}
}

function open_code() {
	code ../../
}

echo "[+] The functions are 'rebuild_inet', 'rebuild_flora' and 'open_code'"
