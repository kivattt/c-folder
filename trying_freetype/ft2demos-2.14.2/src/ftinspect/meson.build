#
# Meson project file for the ftinspect program.
#

# Copyright (C) 2021-2025 by
# David Turner, Robert Wilhelm, and Werner Lemberg.
#
# This file is part of the FreeType project, and may only be used, modified,
# and distributed under the terms of the FreeType project license,
# LICENSE.TXT.  By continuing to use, modify, or distribute this file you
# indicate that you have read the license and understand and accept it
# fully.

iconv_dep = dependency('iconv', required: false)
qt_version_opt = get_option('qt_version')
qt_dep = dependency('', required: false)
qt_mod = disabler()

# Try finding Qt 6
if qt_version_opt == 'auto' or qt_version_opt == 'qt6'
  qt6_mod = import('qt6')
  qt6_dep = dependency('qt6',
    required: qt_version_opt == 'qt6',
    modules: ['Core', 'Gui', 'Widgets'],
    version: '>=6.8')

  if qt6_dep.found()
    qt_mod = qt6_mod
    qt_dep = qt6_dep
  endif
endif

# Try finding Qt 5 if Qt 6 wasn't found or requested
if not qt_dep.found() and (qt_version_opt == 'auto' or qt_version_opt == 'qt5')
  qt5_mod = import('qt5')
  qt5_dep = dependency('qt5',
    required: qt_version_opt == 'qt5',
    modules: ['Core', 'Gui', 'Widgets'],
    version: '>=5.15')

  if qt5_dep.found()
    qt_mod = qt5_mod
    qt_dep = qt5_dep
  endif
endif

# Don't compile `ftinspect` if Qt is not found.  This can happen
# when cross-compiling to a different system.
if qt_dep.found()
  sources = files([
    'engine/charmap.cpp',
    'engine/engine.cpp',
    'engine/fontfilemanager.cpp',
    'engine/fontinfo.cpp',
    'engine/fontinfonamesmapping.cpp',
    'engine/mmgx.cpp',
    'engine/paletteinfo.cpp',
    'engine/rendering.cpp',
    'engine/stringrenderer.cpp',

    'glyphcomponents/glyphbitmap.cpp',
    'glyphcomponents/glyphcontinuous.cpp',
    'glyphcomponents/glyphoutline.cpp',
    'glyphcomponents/glyphpointnumbers.cpp',
    'glyphcomponents/glyphpoints.cpp',
    'glyphcomponents/graphicsdefault.cpp',
    'glyphcomponents/grid.cpp',

    'models/customcomboboxmodels.cpp',
    'models/fontinfomodels.cpp',

    'panels/comparator.cpp',
    'panels/continuous.cpp',
    'panels/glyphdetails.cpp',
    'panels/info.cpp',
    'panels/settingpanel.cpp',
    'panels/settingpanelmmgx.cpp',
    'panels/singular.cpp',

    'widgets/charmapcombobox.cpp',
    'widgets/customwidgets.cpp',
    'widgets/fontsizeselector.cpp',
    'widgets/glyphindexselector.cpp',
    'widgets/tripletselector.cpp',

    'ftinspect.cpp',
    'maingui.cpp',
    'uihelper.cpp',
  ])

  moc_files = qt_mod.preprocess(
    moc_headers: [
      'engine/fontfilemanager.hpp',

      'glyphcomponents/glyphbitmap.hpp',
      'glyphcomponents/glyphcontinuous.hpp',

      'models/customcomboboxmodels.hpp',
      'models/fontinfomodels.hpp',

      'panels/comparator.hpp',
      'panels/continuous.hpp',
      'panels/glyphdetails.hpp',
      'panels/info.hpp',
      'panels/settingpanel.hpp',
      'panels/settingpanelmmgx.hpp',
      'panels/singular.hpp',

      'widgets/charmapcombobox.hpp',
      'widgets/customwidgets.hpp',
      'widgets/fontsizeselector.hpp',
      'widgets/glyphindexselector.hpp',
      'widgets/tripletselector.hpp',

      'maingui.hpp',
    ],
    dependencies: qt_dep)

  executable('ftinspect',
    sources,
    moc_files,
    dependencies: [qt_dep, libfreetype2_dep],
    override_options: ['cpp_std=c++17'],
    install: true)
endif

# EOF
