echo "[+] Sourcing the setenvs"
source $PWD/../../setenv
source $PWD/../inet4.4/setenv

echo "[+] Setting functions"
CORES=$(nproc)

function rebuild_inet() {
	OLDPWD=$PWD && \
	cd $PWD/../inet4.4 && \
	make clean && \
	make MODE=release -j${CORES} && \
	make MODE=debug -j${CORES} && \
	cd $OLDPWD
}

function rebuild_flora() {
	make clean && \
	make MODE=release -j${CORES} && \
	make MODE=debug -j${CORES}
}

echo "[+] The functions are 'rebuild_inet' and 'rebuild_flora'"
