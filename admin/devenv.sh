#!/bin/bash -x

SCRIPT_DIR=$(cd $(dirname ${BASH_SOURCE}[0]) && pwd)                           # get current directory
source ${SCRIPT_DIR}/dependencies $*

# defaults for dev
[[ -z ${INSTALL_BASE} ]] && export INSTALL_BASE="${HOME}/install"

#################################################################
# look for files of the form devenv.$user.$host.sh and devenv.$user.sh
#################################################################
rc=0
HOST=`hostname -s`
if [[ -e ${SCRIPT_DIR}/devenv.${USER}.${HOST}.sh ]]; then
	echo "using ${SCRIPT_DIR}/devenv.${USER}.${HOST}.sh"
	source ${SCRIPT_DIR}/devenv.${USER}.${HOST}.sh
	rc=$?
elif 	[[ -e ${SCRIPT_DIR}/devenv.${USER}.sh ]]; then
	echo "using ${SCRIPT_DIR}/devenv.${USER}.sh"
	source ${SCRIPT_DIR}/devenv.${USER}.sh
	rc=$?
fi

#################################################################
# everything from here down depends on variables set above
#################################################################
# use above to set environment
[[ $rc -ne 255 ]] && source ${SCRIPT_DIR}/devenv.common.sh
