#!/usr/bin/env bash

PROGRAM=opp_run

CONFIGFILE=flora-tdma.ini

SIMENV=Qtenv

NEDPATHS=(
	.
	../src
	../../inet4.4/examples
	../../inet4.4/showcases
	../../inet4.4/src
	../../inet4.4/tests/validation
	../../inet4.4/tests/networks
	../../inet4.4/tutorials
)

EXCLUDENEDPACKAGES=(
	inet.common.selfdoc
	inet.linklayer.configurator.gatescheduling.z3
	inet.emulation
	inet.showcases.visualizer.osg
	inet.examples.emulation
	inet.showcases.emulation
	inet.transportlayer.tcp_lwip
	inet.applications.voipstream
	inet.visualizer.osg
	inet.examples.voipstream
)

IMAGEPATH=../../inet4.4/images

LIBRARIES=(
	../src/flora-tdma
	../../inet4.4/src/INET
)

OPTS=(
	-m
	-u ${SIMENV}
	-n $(echo ${NEDPATHS[@]} | sed 's/ /:/g')
	-x \"$(echo ${EXCLUDENEDPACKAGES[@]} | sed 's/ /;/g')\"
	--image-path=${IMAGEPATH}
)

for lib in "${LIBRARIES[@]}"; do
	OPTS+=(-l ${lib})
done


CMD="${PROGRAM} ${OPTS[@]} ${CONFIGFILE}"

$CMD
