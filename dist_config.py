import os

package = 'libt3config'
excludesrc = '/(Makefile|TODO.*|SciTE.*|run\.sh|test\.c)$'
auxsources = [ 'src/.objects/*_hide.h', 'src/.objects/*.bytes', 'src/config_api.h', 'src/config_errors.h', 'src/config_shared.c' ]
auxfiles = [ 'doc/API' ]
extrabuilddirs = [ 'doc' ]

versioninfo = '0:1:0'

def get_replacements(mkdist):
	return [
		{
			'tag': '<VERSION>',
			'replacement': mkdist.version
		},
		{
			'tag': '^#define T3_CONFIG_VERSION .*',
			'replacement': '#define T3_CONFIG_VERSION ' + mkdist.get_version_bin(),
			'files': [ 'src/config.h' ],
			'regex': True
		},
		{
			'tag': '<OBJECTS>',
			'replacement': " ".join(mkdist.sources_to_objects(mkdist.sources, '\.c$', '.lo')),
			'files': [ 'Makefile.in' ]
		},
		{
			'tag': '<VERSIONINFO>',
			'replacement': versioninfo,
			'files': [ 'Makefile.in' ]
		},
		{
			'tag': '<LIBVERSION>',
			'replacement': versioninfo.split(':', 2)[0],
			'files': [ 'Makefile.in' ]
		},
		{
			'tag': '.objects/',
			'replacement': '',
			'files': [ 'src/parser.c' ]
		}
	]

def finalize(mkdist):
	os.symlink('.', os.path.join(mkdist.topdir, 'src', 't3config'))
