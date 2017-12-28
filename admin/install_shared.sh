#!/bin/bash -x

source ${BUILD_DIR}/admin/dependencies $*
source ${BUILD_DIR}/admin/devenv.buildmaster.sh

# repoint the high-level link
# i.e., ${VERSION} ===> ${VERSION}-${BUILD_NUMBER}
rm -f ${INSTALL_BASE}/${PROJECT_NAME}/${VERSION}
ln -s ${INSTALL_BASE}/${PROJECT_NAME}/${VERSION}-${BUILD_NUMBER} ${INSTALL_BASE}/${PROJECT_NAME}/${VERSION}

#####################################################################################
# create tag in svn
SVN_URL=$(dirname $(dirname $(svn info ${SRC_DIR} | grep 'URL' | awk '{print $2}')))
# if a later step (e.g. build_rpm.sh) fails, subsequent invocations of svn copy will copy INTO existing tag rather than creating a new tag
svn rm                        ${SVN_URL}/tags/${PROJECT_NAME}-${VERSION}-${BUILD_NUMBER} -m "[${JIRA_RELEASE}]: auto-create tag for build"
svn copy --parents ${SRC_DIR} ${SVN_URL}/tags/${PROJECT_NAME}-${VERSION}-${BUILD_NUMBER} -m "[${JIRA_RELEASE}]: auto-create tag for build"
rc=$?
if [ $rc != 0 ]; then
   echo " Error ($rc): Could not create svn tag for ${SVN_URL}/tags/${Project_Short_Name}-${VERSION}-${BUILD_NUMBER}"
  exit $rc;
fi

exit 0