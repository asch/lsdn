#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# LSDN documentation build configuration file, created by
# sphinx-quickstart on Fri Jan 26 15:04:58 2018.
#
# This file is execfile()d with the current directory set to its
# containing dir.
#
# Note that not all possible configuration values are present in this
# autogenerated file.
#
# All configuration values have a default; values that are commented out
# serve to show the default.

import os
import subprocess
import sys
sys.path.insert(0, os.path.abspath('.'))


# -- General configuration ------------------------------------------------

# If your documentation needs a minimal Sphinx version, state it here.
#
# needs_sphinx = '1.0'

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = ['sphinx.ext.todo', 'sphinx.ext.graphviz', 'breathe',
        'sphinx_lsctl', 'sphinx.ext.ifconfig', 'sphinxcontrib.spelling']

# Add any paths that contain templates here, relative to this directory.
templates_path = ['_templates']

# The suffix(es) of source filenames.
# You can specify multiple suffix as a list of string:
#
# source_suffix = ['.rst', '.md']
source_suffix = '.rst'

# The master toctree document.
master_doc = 'index'

# General information about the project.
project = 'LSDN'
copyright = '2018, LSDN Collective'
author = 'LSDN Collective'

# The version info for the project you're documenting, acts as replacement for
# |version| and |release|, also used in various other places throughout the
# built documents.
#
# The short X.Y version.
version = ''
# The full version, including alpha/beta/rc tags.
release = ''

# The language for content autogenerated by Sphinx. Refer to documentation
# for a list of supported languages.
#
# This is also used if you do content translation via gettext catalogs.
# Usually you set "language" from the command line for these cases.
language = None

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This patterns also effect to html_static_path and html_extra_path
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

# The name of the Pygments (syntax highlighting) style to use.
pygments_style = 'sphinx'

# If true, `todo` and `todoList` produce output, else they produce nothing.
todo_include_todos = True

breathe_projects = {
    'lsdn': '../doxygen/xml'
}
breathe_domain_by_extension = {
    'h': 'c',
}
breathe_default_project = 'lsdn'

default_role = 'ref'


# -- Options for HTML output ----------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = 'haiku'
html_logo = 'logo.svg'
html_style = 'theme.css'

# Theme options are theme-specific and customize the look and feel of a theme
# further.  For a list of options available for each theme, see the
# documentation.
#
# html_theme_options = {}

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['_static']

# Custom sidebar templates, must be a dictionary that maps document names
# to template names.
#
# This is required for the alabaster theme
# refs: http://alabaster.readthedocs.io/en/latest/installation.html#sidebars
html_sidebars = {
    '**': [
        'relations.html',  # needs 'show_related': True theme option to display
        'searchbox.html',
    ]
}


# -- Options for HTMLHelp output ------------------------------------------

# Output file base name for HTML help builder.
htmlhelp_basename = 'lsdndoc'


# -- Options for LaTeX output ---------------------------------------------
latex_engine = 'pdflatex'
latex_elements = {
	'papersize': 'a4paper',
	'pointsize': '12pt',
	'preamble': r'''
	    \usepackage{tgpagella}
	    \usepackage[T1]{fontenc}
	    \usepackage{DejaVuSansMono}
	''',
        'sphinxsetup': 'verbatimwithframe=false,VerbatimColor={RGB}{233,239,255}'
}

# Grouping the document tree into LaTeX files. List of tuples
# (source start file, target name, title,
#  author, documentclass [howto, manual, or own class]).
latex_documents = [
    (master_doc, 'lsdn.tex', 'LSDN Documentation',
     'LSDN Collective', 'manual'),
]

def generate_doxygen_xml(app):
    """Run the doxygen make commands if we're on the ReadTheDocs server"""

    if os.environ.get('READTHEDOCS', None) == 'True':
        try:
            retcode = subprocess.call("cd ..; doxygen doxygen/Doxyfile", shell=True)
            if retcode < 0:
                sys.stderr.write("doxygen terminated by signal %s" % (-retcode))
        except OSError as e:
            sys.stderr.write("doxygen execution failed: %s" % e)

def setup(app):
    # Add hook for building doxygen xml when needed
    app.connect("builder-inited", generate_doxygen_xml)
