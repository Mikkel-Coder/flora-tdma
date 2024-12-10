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
	echo -e "\n\n[+] RELEASE MODE:" && \
	make MODE=release -j${CORES} && \
	echo -e "\n\n[+] DEBUG MODE:" && \
	make MODE=debug -j${CORES} && \
	cd $OLDPWD
}

function rebuild_flora() {
	make makefiles && \
	make clean && \
	echo -e "\n\n[+] RELEASE MODE:" && \
	make MODE=release -j${CORES} && \
	echo -e "\n\n[+] DEBUG MODE:" && \
	make MODE=debug -j${CORES}
}

function open_code() {
	code ../../
}

function tdmarun() {
	OLDPWD=$PWD && \
	cd $PWD/simulations && \
	./tdmarun.sh && \
	cd $OLDPWD
}

echo "[+] The functions are 'rebuild_inet', 'rebuild_flora', 'open_code' and 'tdmarun'"
