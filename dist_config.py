import glob
import subprocess
import os

package = 'libt3config'
excludesrc = '/(Makefile|TODO.*|SciTE.*|run\.sh|test\.c)$'
auxsources = [ 'src/.objects/*_hide.h', 'src/.objects/*.bytes', 'src/config_api.h', 'src/config_errors.h', 'src/config_shared.c' ]
auxfiles = [ 'doc/API' ]

versioninfo = '0:1:0'

def build(mkdist):
	subprocess.call(['make', '-C', 'doc', 'clean'])
	subprocess.call(['make', '-C', 'doc'])

def get_replacements(mkdist):
	objects = mkdist.include_by_regex(mkdist.sources, '\.c$')
	objects = mkdist.regex_replace(objects, '/\.objects/', '/')
	objects = mkdist.regex_replace(objects, '\.c$', '.lo')
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
			'replacement': " ".join(objects),
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
	os.symlink('.', os.path.join(mkdist.topdir, 't3config'))
