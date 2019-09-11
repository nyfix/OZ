#!/bin/bash -x

source ${BUILD_DIR}/admin/dependencies $*
source ${BUILD_DIR}/admin/devenv.buildmaster.sh

# repoint the high-level link
# i.e., ${VERSION} ===> ${VERSION}-${BUILD_NUMBER}
rm -f ${INSTALL_BASE}/${PROJECT_NAME}/${VERSION}
ln -s ${INSTALL_BASE}/${PROJECT_NAME}/${VERSION}-${BUILD_NUMBER} ${INSTALL_BASE}/${PROJECT_NAME}/${VERSION}

#####################################################################################
# create tag in git
BUILD="${VERSION}-${BUILD_NUMBER}"

# remove tag from remote; retag and push tag to remote
git push origin :refs/tag/${BUILD}
git tag -f -a ${BUILD} -m "[${BUILD}]: auto-create tag for build" && git push -f origin ${BUILD}
rc=$?
if [ $rc != 0 ]; then
   echo " Error ($rc): Could not create git tag for ${PROJECT_NAME}-${BUILD}}"
   exit $rc;
fi

exit 0
