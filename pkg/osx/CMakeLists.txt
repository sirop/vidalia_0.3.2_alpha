##
##  $Id$
##
##  This file is part of Vidalia, and is subject to the license terms in the
##  LICENSE file, found in the top level directory of this distribution. If
##  you did not receive the LICENSE file with this file, you may obtain it
##  from the Vidalia source package distributed by the Vidalia Project at
##  http://www.torproject.org/projects/vidalia.html. No part of Vidalia,
##  including this file, may be copied, modified, propagated, or distributed
##  except according to the terms described in the LICENSE file.
##


set(VIDALIA_APP_BUNDLE_ROOT ${Vidalia_BINARY_DIR}/src/vidalia/Vidalia.app)
get_target_property(VIDALIA_EXECUTABLE Vidalia LOCATION)
add_custom_target(dist-osx-libraries DEPENDS Vidalia)
if (QT_USE_FRAMEWORKS)
  vidalia_copy_plugins(
    TARGET dist-osx-libraries
    DIR ${SCRIPT_DIR}
    APP_BUNDLE ${VIDALIA_APP_BUNDLE_ROOT}
    COMPONENTS core gui network uitools xml
  )
  vidalia_do_plugins(
    TARGET dist-osx-libraries
    DIR ${SCRIPT_DIR}
    APP_BUNDLE ${VIDALIA_APP_BUNDLE_ROOT}
    FRAMEWORKS QtGui QtCore QtScript QtNetwork QtXml
    COMPONENTS core gui network uitools xml
  )
  if (USE_MARBLE)
    vidalia_install_dylib(MarbleWidget
      TARGET dist-osx-libraries
      LIBRARY ${MARBLEWIDGET_LIBRARY}
      APP_BUNDLE ${VIDALIA_APP_BUNDLE_ROOT}
      DEPENDS_FRAMEWORKS ${QtCore} ${QtGui} ${QtNetwork} ${QtXml} ${QtSvg}
                         ${QtDBus} ${QtScript} ${QtWebKit}
    )
    vidalia_install_name_tool(${VIDALIA_EXECUTABLE}
      TARGET dist-osx-libraries
      LIBRARIES ${MARBLEWIDGET_LIBRARY}
    )
  endif(USE_MARBLE)
endif(QT_USE_FRAMEWORKS)

if (OSX_FAT_BINARY)
  set(DMG_ARCH "universal")
else(OSX_FAT_BINARY)
  set(DMG_ARCH ${CMAKE_OSX_ARCHITECTURES})
endif(OSX_FAT_BINARY)

add_custom_target(dist-osx
  COMMAND ${QT_BINARY_DIR}/macdeployqt ${VIDALIA_APP_BUNDLE_ROOT}
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/pkg-dmg
    --source ${VIDALIA_APP_BUNDLE_ROOT}
    --target ${Vidalia_BINARY_DIR}/vidalia-${VERSION}-${DMG_ARCH}.dmg
    --sourcefile --volname "Vidalia ${VERSION}"
    --icon ${Vidalia_SOURCE_DIR}/src/vidalia/res/icons/vidalia.icns
    --symlink "/Applications:/Drag to here"
    --mkdir /.background
    --copy ${CMAKE_CURRENT_SOURCE_DIR}/background.png:/.background/
    --copy ${CMAKE_CURRENT_SOURCE_DIR}/nonbundle-ds_store:/.DS_Store
    --copy ${CMAKE_CURRENT_SOURCE_DIR}/qt.conf:/Vidalia.app/Contents/Resources/qt.conf
    --mkdir /Licenses
    --copy ${Vidalia_SOURCE_DIR}/LICENSE:/Licenses/License.txt
    --copy ${Vidalia_SOURCE_DIR}/LICENSE-GPLV2:/Licenses/License-GPLv2.txt
    --copy ${Vidalia_SOURCE_DIR}/LICENSE-GPLV3:/Licenses/License-GPLv3.txt
    --copy ${Vidalia_SOURCE_DIR}/LICENSE-OPENSSL:/Licenses/License-OpenSSL.txt
    --copy ${Vidalia_SOURCE_DIR}/README:/ReadMe.txt
    --copy ${Vidalia_SOURCE_DIR}/CHANGELOG:/Changes.txt
  COMMENT "Creating vidalia-${VERSION}-${DMG_ARCH}.dmg"
  VERBATIM
)
add_dependencies(dist-osx dist-osx-libraries)

if (TOR_SOURCE_DIR AND SCRIPT_DIR)
  get_filename_component(TOR_SOURCE_DIR ${TOR_SOURCE_DIR} ABSOLUTE)

  ## Determine Tor's version string used to build the .dmg filename
  vidalia_get_tor_version(TOR_VERSION "${TOR_SOURCE_DIR}/src/or/tor")
  if (TOR_VERSION)
    message(STATUS "Found Tor ${TOR_VERSION}")
  else (TOR_VERSION)
    message(FATAL_ERROR "Unable to determine version of ${TOR_SOURCE_DIR}/src/or/tor")
  endif(TOR_VERSION)

  set(bundle_DMG "vidalia-bundle-${TOR_VERSION}-${VERSION}-${DMG_ARCH}.dmg")
  add_custom_target(dist-osx-bundle
    COMMAND ${QT_BINARY_DIR}/macdeployqt ${VIDALIA_APP_BUNDLE_ROOT}
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/pkg-dmg
      --source ${VIDALIA_APP_BUNDLE_ROOT}
      --target ${Vidalia_BINARY_DIR}/${bundle_DMG}
      --sourcefile --volname "Vidalia Bundle ${VERSION}"
      --icon ${Vidalia_SOURCE_DIR}/src/vidalia/res/icons/vidalia.icns
      --symlink "/Applications:/Drag to here"
      --mkdir /.background
      --copy ${CMAKE_CURRENT_SOURCE_DIR}/bundle-background.png:/.background/
      --copy ${CMAKE_CURRENT_SOURCE_DIR}/bundle-ds_store:/.DS_Store
      --copy ${CMAKE_CURRENT_SOURCE_DIR}/vidalia.conf.sample:/Vidalia.app/Contents/Resources/vidalia.conf
      --copy ${CMAKE_CURRENT_SOURCE_DIR}/qt.conf:/Vidalia.app/Contents/Resources/qt.conf
      --mkdir /Licenses
      --copy ${Vidalia_SOURCE_DIR}/LICENSE:/Licenses/License.txt
      --copy ${Vidalia_SOURCE_DIR}/LICENSE-GPLV2:/Licenses/License-GPLv2.txt
      --copy ${Vidalia_SOURCE_DIR}/LICENSE-GPLV3:/Licenses/License-GPLv3.txt
      --copy ${Vidalia_SOURCE_DIR}/LICENSE-OPENSSL:/Licenses/License-OpenSSL.txt
      --copy ${Vidalia_SOURCE_DIR}/CHANGELOG:/Changes.txt
      --copy ${TOR_SOURCE_DIR}/src/or/tor:/Vidalia.app/Contents/MacOS/
      --copy ${TOR_SOURCE_DIR}/src/tools/tor-checkkey:/Vidalia.app/Contents/MacOS/
      --copy ${TOR_SOURCE_DIR}/src/tools/tor-gencert:/Vidalia.app/Contents/MacOS/
      --copy ${TOR_SOURCE_DIR}/src/tools/tor-resolve:/Vidalia.app/Contents/MacOS/
      --copy ${TOR_SOURCE_DIR}/src/config/geoip:/Vidalia.app/share/tor/
      --copy ${TOR_SOURCE_DIR}/src/config/torrc.sample:/Vidalia.app/Contents/Resources/
    COMMENT "Creating ${bundle_DMG}"
    VERBATIM
  )
  add_dependencies(dist-osx-bundle dist-osx-libraries)

  add_custom_target(dist-osx-split-bundle
    COMMAND hdiutil segment -segmentSize 1.5m
      -o "${Vidalia_BINARY_DIR}/split-${bundle_DMG}" "${Vidalia_BINARY_DIR}/${bundle_DMG}"
    COMMENT "Creating split-${bundle_DMG}"
    VERBATIM
  )
  add_dependencies(dist-osx-split-bundle dist-osx-bundle)
else(TOR_SOURCE_DIR AND SCRIPT_DIR)
  message("-- Skipping dist-osx-bundle build target")
  add_custom_target(dist-osx-bundle
    COMMAND ${CMAKE_COMMAND} -E echo
      "!! The dist-osx-bundle target is not configured."
    COMMAND ${CMAKE_COMMAND} -E echo
      "!! You must set the -DTOR_SOURCE_DIR and -DSCRIPT_DIR CMake variables."
    COMMAND ${CMAKE_COMMAND} -E echo "!! Quitting."
  )
  add_custom_target(dist-osx-split-bundle
    COMMAND ${CMAKE_COMMAND} -E echo
      "!! The dist-osx-split-bundle target is not configured. Quitting."
    COMMAND ${CMAKE_COMMAND} -E echo
      "!! You must set the -DTOR_SOURCE_DIR and -DSCRIPT_DIR CMake variables."
    COMMAND ${CMAKE_COMMAND} -E echo "!! Quitting."
  )
endif(TOR_SOURCE_DIR AND SCRIPT_DIR)

