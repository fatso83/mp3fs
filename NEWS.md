# mp3fs News

## Important changes in 1.1.1 (2021-03-08)

The only change in this release is a fix for an issue with filenames containing
square brackets.

## Important changes in 1.1 (2021-01-30)

This contains various bug fixes, mostly.

Fixes/improvements:

- Recognize upper/mixed case in input filenames
- Fix file size handling with and without cache
- Various code simplifications/cleanup

New features:

- Log lines now include thread ID and can be customized.

## Important changes in 1.0 (2020-05-24)

mp3fs 1.0 is finally here!

Fixes/code improvements:

- Many, many bug fixes (buffer overflows, memory leaks, and others)
- Memory handling improvements (using RAII with C++ nearly everywhere)
- Adopting C++11 and modernizing code
- Static tests for code (e.g. clang-format, clang-tidy, IWYU)

New features:

- Ogg Vorbis decoding support
- MP3 VBR encoding support
- Improved, much more customizable logging

Other:

- Docs licensed under GPL 3+ (now entire codebase is distributable as GPL 3)
- All docs switched to Markdown (including manpage, using pandoc)
- Docker image now available

## Important changes in 0.91 (2014-05-14)

This contains mainly bug fixes.

Changes in this release:

- Fixed a segfault caused by an overflow reading the list of available
  decoders.
- A number of problems with the previous distribution tar are now fixed.
- The output of `mp3fs --version` has been made more complete.

## Important changes in 0.9 (2014-04-06)

This is a major new release, and brings us very close to a 1.0 release!

Changes in this release:

- All transcoding code has been completely rewritten. Encoding and decoding
  have been abstracted out into base classes defining interfaces that can be
  implemented by different codec classes, with just a FLAC decoder and MP3
  encoder at the moment.
- The build system has been modified as well to support this usage.
- A number of small bugs or code inefficiencies have been fixed.

## Important changes in 0.32 (2012-06-18)

This release has a lot of bug fixes and some code cleanup.

Changes in this release:

- The file size calculation should always be correct.
- A crash affecting programs like scp that might try to access past the end of
  the file has been fixed.
- Too many other little fixes were made to list here. See the ChangeLog for
  full details.

## Important changes in 0.31 (2011-12-04)

This is a minor update, with bug fixes and a new feature.

Changes in this release:

- The ReplayGain support added earlier now can be configured through the
  command line.
- Filename translation (from .flac to .mp3) is now fixed on filesystems such as
  XFS that do not populate dirent.d_type.
- A couple other minor bugs fixes and changes were made.

## Important changes in 0.30 (2010-12-01)

This is a major new release, and brings mp3fs much closer to an eventual 1.0
release.

Changes in this release:

- Support for additional metadata tags has been added. (From Gregor Zurowski)
- Documentation improvements: the help message is more useful, and a man page
  has been added.
- Choosing bitrate is now done with a command-line or mount option, rather than
  the old comma syntax.
- A new option to select LAME encoding quality is now available. (From Gregor
  Zurowski)
- Debug output can be enabled at runtime.
- Old external libraries included in distribution (StringIO, talloc) have been
  removed and replaced.
- Numerous bug fixes have been made. (Some from Gregor Zurowski)

...

## 0.01 Initial release (2006-08-06)

## License

Copyright (C) 2010-2014 K. Henriksson

This documentation may be distributed under the GNU Free Documentation License
(GFDL) 1.3 or later with no invariant sections, or alternatively under the GNU
General Public License (GPL) version 3 or later.
