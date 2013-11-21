Minidump Unittest port to Windows. v 0.1.

The minidump_file_writer_unittest is located here [1]. The project was previously developed on Linux [2].
It contains 4 files.

1. minidump_file_writer_unittest.cc
2. minidump_file_writer.h
3. minidump_file_writer.cc
4. minidump_file_writer-inl.h

To compile this project on Windows, GYP was used to generate Visual Studio project files.

The gyp file is located here [3]. Generate the project with this command:

breakpad\src\tools\gyp\gyp.bat --no-circular-check src\client\minidump_file_writer.gyp

Details:

Choice between Windows.h or io.h:

The header file io.h shipped with Windows is a wrapper for Windows API. It has function definitions
similar to POSIX, but Windows.h provides more control being more low level. This is the reason Windows.h
was used for filing purposes.


POSIX replacements with Window API:

The 4 files mentioned above use these 4 POSIX functions:

1. int open(const char *pathname,
int flags, mode_t mode) [4]; /* function used to open files, at various points sys_open is also used but no explanation is given */
2. ssize_t read(int fd, void *buf, size_t count) [5]; /* function used to read a file */
3. ssize_t write(int fd, const void *buf, size_t count) [6];  /* function used to write to a file */
2. int close(int fd) [7]; /* function used to close a file */
3. int ftruncate(int fd, off_t length) [8];  /* function used to truncate the file  */
4. off_t lseek(int fd, off_t offset, int whence) [9]; /* move in file at the very basic */

These functions were replaced with Windows API functions

1. CreateFile [10]
2. CloseHandle [11]
3. WriteFile [12]
4. ReadFile [13]
5. SetFilePointer [14] 
6. SetFilePoinerEx [15] *
7. SetEndOfFile [16]

* The difference between SetFilePointer and SetFilePointer is explained in the code.
The usage details of each of these functions is explained in the code.

References:

[1]   https://code.google.com/p/google-breakpad/source/browse/trunk/src/client/?r=1018

[2]   The original unittest doesn't run. You need to include stdlib and string headers.

[3]   https://github.com/salmanjavaid/break_pad_v_0.1/tree/master/breakpad/src/client/windows/unittests

[4]   http://linux.die.net/man/2/open

[5]   http://linux.die.net/man/2/read

[6]   http://linux.die.net/man/2/write

[7]   http://linux.die.net/man/2/close

[8]   http://linux.die.net/man/2/ftruncate

[9]   http://linux.die.net/man/2/lseek

[10]  http://msdn.microsoft.com/en-us/library/windows/desktop/aa363858%28v=vs.85%29.aspx

[11]  http://msdn.microsoft.com/en-us/library/windows/desktop/ms724211%28v=vs.85%29.aspx

[12]  http://msdn.microsoft.com/en-us/library/windows/desktop/aa365747%28v=vs.85%29.aspx

[13]  http://msdn.microsoft.com/en-us/library/windows/desktop/aa365467%28v=vs.85%29.aspx

[14]  http://msdn.microsoft.com/en-us/library/windows/desktop/aa365541%28v=vs.85%29.aspx

[15]  http://msdn.microsoft.com/en-us/library/windows/desktop/aa365542%28v=vs.85%29.aspx

[17]  http://msdn.microsoft.com/en-us/library/windows/desktop/aa365531%28v=vs.85%29.aspx