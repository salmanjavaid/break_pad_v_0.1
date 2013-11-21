// Copyright (c) 2006, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// minidump_file_writer.cc: Minidump file writer implementation.
//
// See minidump_file_writer.h for documentation.

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <windows.h>
#include <io.h>
#endif


#include "client/minidump_file_writer-inl.h"
#include "common/linux/linux_libc_support.h"
#include "common/string_conversion.h"
#if __linux__
#include "third_party/lss/linux_syscall_support.h"
#endif

namespace google_breakpad {

const MDRVA MinidumpFileWriter::kInvalidMDRVA = static_cast<MDRVA>(-1);

MinidumpFileWriter::MinidumpFileWriter()
    : file_(-1),
      close_file_when_destroyed_(true),
      position_(0),
      size_(0), hFile_(NULL) {
}

MinidumpFileWriter::~MinidumpFileWriter() {
  if (close_file_when_destroyed_)
    Close();
}


bool MinidumpFileWriter::Open(const char *path) {

#if __linux__
  assert(file_ == -1);
  file_ = sys_open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
#elif _WIN32
  hFile_ = CreateFile(
	  path,  // the location of file
	  GENERIC_WRITE,  // allow to write to file
	  FILE_SHARE_WRITE,  // make sure no other process can access this file
	  NULL, // this is a security attribute
	  CREATE_ALWAYS, // always create a new file, overwrite if already exist
	  FILE_ATTRIBUTE_NORMAL, // a normal file
	  NULL // extended attributes for file
	  );

  if (hFile_ == INVALID_HANDLE_VALUE) {  // if the CreateFile failed
	  return false;
  } else { 
	  return true; 
  }

#else
  assert(file_ == -1);
  file_ = open(path, O_WRONLY | O_CREAT | O_EXCL, 0600);
#endif

  return file_ != -1;
}

void MinidumpFileWriter::SetFile(const int file) {
  assert(file_ == -1);
  file_ = file;
  close_file_when_destroyed_ = false;
}

bool MinidumpFileWriter::Close() {
  bool result = true;

#ifndef _WIN32
  if (file_ != -1) {
    if (-1 == ftruncate(file_, position_)) {
       return false;
    }

#if __linux__
    result = (sys_close(file_) == 0);
#else
    result = (close(file_) == 0);
#endif
    file_ = -1;
  }
#else
  if (hFile_ != INVALID_HANDLE_VALUE) {
		LARGE_INTEGER li; //LARGE_INETEGR to tell how many bytes
		li.QuadPart = position_; // SetFilePointerEx expected a large integer
		/* this function returns a bool if the operation has succedded */
		bool flag_SetFilePointer =  SetFilePointerEx(
		  hFile_, // handle to opened file
		  li, // the number of bytes to move
		  NULL, // if you want to receive a new file pointer
		  0 // starting point for new file pointer
		);

		if (flag_SetFilePointer == true) { // if the previous operation completed successfully
			bool flag_SetEndOfFile = SetEndOfFile( // truncate file
			  hFile_ // pass the handle
			);
	
			if (flag_SetEndOfFile == true) { // if previous function completed successfully
				return CloseHandle(hFile_); // close the file
			} else {
				return false; // else return false if failed
			}

		} else {
			return false; // else return false if failed
		}
  }

#endif
  return result;

}


/* clone of getpagesize() on Linux http://www.genesys-e.org/jwalter/mix4win.htm */
#ifdef _WIN32
long MinidumpFileWriter::getpagesize_WIN (void) {
    static long g_pagesize = 0;
    if (! g_pagesize) {
        SYSTEM_INFO system_info;
        GetSystemInfo (&system_info);
        g_pagesize = system_info.dwPageSize;
    }
    return g_pagesize;
}
#endif

bool MinidumpFileWriter::CopyStringToMDString(const wchar_t *str,
                                              unsigned int length,
                                              TypedMDRVA<MDString> *mdstring) {
  bool result = true;
  if (sizeof(wchar_t) == sizeof(uint16_t)) {
    // Shortcut if wchar_t is the same size as MDString's buffer
    result = mdstring->Copy(str, mdstring->get()->length);
  } else {
    uint16_t out[2];
    int out_idx = 0;

    // Copy the string character by character
    while (length && result) {
		google_breakpad::UTF32ToUTF16Char(*str, out);
      if (!out[0])
        return false;

      // Process one character at a time
      --length;
      ++str;

      // Append the one or two UTF-16 characters.  The first one will be non-
      // zero, but the second one may be zero, depending on the conversion from
      // UTF-32.
      int out_count = out[1] ? 2 : 1;
      size_t out_size = sizeof(uint16_t) * out_count;
      result = mdstring->CopyIndexAfterObject(out_idx, out, out_size);
      out_idx += out_count;
    }
  }
  return result;
}

bool MinidumpFileWriter::CopyStringToMDString(const char *str,
                                              unsigned int length,
                                              TypedMDRVA<MDString> *mdstring) {
  bool result = true;
  uint16_t out[2];
  int out_idx = 0;

  // Copy the string character by character
  while (length && result) {
    int conversion_count = google_breakpad::UTF8ToUTF16Char(str, length, out);
    if (!conversion_count)
      return false;

    // Move the pointer along based on the nubmer of converted characters
    length -= conversion_count;
    str += conversion_count;

    // Append the one or two UTF-16 characters
    int out_count = out[1] ? 2 : 1;
    size_t out_size = sizeof(uint16_t) * out_count;
    result = mdstring->CopyIndexAfterObject(out_idx, out, out_size);
    out_idx += out_count;
  }
  return result;
}

template <typename CharType>
bool MinidumpFileWriter::WriteStringCore(const CharType *str,
                                         unsigned int length,
                                         MDLocationDescriptor *location) {
  assert(str);
  assert(location);
  // Calculate the mdstring length by either limiting to |length| as passed in
  // or by finding the location of the NULL character.
  unsigned int mdstring_length = 0;
  if (!length)
    length = INT_MAX;
  for (; mdstring_length < length && str[mdstring_length]; ++mdstring_length)
    ;

  // Allocate the string buffer
  TypedMDRVA<MDString> mdstring(this);
  if (!mdstring.AllocateObjectAndArray(mdstring_length + 1, sizeof(uint16_t)))
    return false;

  // Set length excluding the NULL and copy the string
  mdstring.get()->length =
      static_cast<uint32_t>(mdstring_length * sizeof(uint16_t));
  bool result = CopyStringToMDString(str, mdstring_length, &mdstring);

  // NULL terminate
  if (result) {
    uint16_t ch = 0;
    result = mdstring.CopyIndexAfterObject(mdstring_length, &ch, sizeof(ch));

    if (result)
      *location = mdstring.location();
  }

  return result;
}

bool MinidumpFileWriter::WriteString(const wchar_t *str, unsigned int length,
                 MDLocationDescriptor *location) {
  return WriteStringCore(str, length, location);
}

bool MinidumpFileWriter::WriteString(const char *str, unsigned int length,
                 MDLocationDescriptor *location) {
  return WriteStringCore(str, length, location);
}

bool MinidumpFileWriter::WriteMemory(const void *src, size_t size,
                                     MDMemoryDescriptor *output) {
  assert(src);
  assert(output);
  UntypedMDRVA mem(this);

  if (!mem.Allocate(size))
    return false;
  if (!mem.Copy(src, mem.size()))
    return false;

  output->start_of_memory_range = reinterpret_cast<uint64_t>(src);
  output->memory = mem.location();

  return true;
}

MDRVA MinidumpFileWriter::Allocate(size_t size) {
  assert(size);
#ifndef _WIN32
  assert(file_ != -1);
#endif
  size_t aligned_size = (size + 7) & ~7;  // 64-bit alignment
#ifndef _WIN32
  if (position_ + aligned_size > size_) {
    size_t growth = aligned_size;
    size_t minimal_growth = getpagesize();

    // Ensure that the file grows by at least the size of a memory page
    if (growth < minimal_growth)
      growth = minimal_growth;

    size_t new_size = size_ + growth;
    if (ftruncate(file_, new_size) != 0)
      return kInvalidMDRVA;

    size_ = new_size;
  }
#else
    if (position_ + aligned_size > size_) {
		size_t growth = aligned_size;
		size_t minimal_growth = getpagesize_WIN();

		// Ensure that the file grows by at least the size of a memory page
		if (growth < minimal_growth)
			growth = minimal_growth;

		size_t new_size = size_ + growth;

		LARGE_INTEGER li; //LARGE_INETEGR to tell how many bytes
		li.QuadPart = new_size; // SetFilePointerEx expected a large integer
		bool flag_SetFilePointer =  SetFilePointerEx(
			hFile_, // handle to opened file
			li, // the number of bytes to move
			NULL, // if you want to receive a new file pointer
			0 // starting point for new file pointer
		);

		if (flag_SetFilePointer == true) { // if the previous operation completed successfully
			bool flag_SetEndOfFile = SetEndOfFile( // truncate file
				hFile_ // pass the handle
			);
	
			if (flag_SetEndOfFile == false) { // if previous function failed
				return  kInvalidMDRVA; // function failed
			}
		size_ = new_size;
		} else { 
			return  kInvalidMDRVA; // function failed
		}
	}

#endif
  MDRVA current_position = position_;
  position_ += static_cast<MDRVA>(aligned_size);

  return current_position;
}

#ifndef _WIN32
bool MinidumpFileWriter::Copy(MDRVA position, const void *src, ssize_t size) {
  assert(src);
  assert(size);
  assert(file_ != -1);

  // Ensure that the data will fit in the allocated space
  if (static_cast<size_t>(size + position) > size_)
    return false;

  // Seek and write the data
#if __linux__
  if (sys_lseek(file_, position, SEEK_SET) == static_cast<off_t>(position)) {
    if (sys_write(file_, src, size) == size) {
#else
  if (lseek(file_, position, SEEK_SET) == static_cast<off_t>(position)) {
    if (write(file_, src, size) == size) {
#endif
      return true;
    }
  }

  return false;
}
#else
bool MinidumpFileWriter::Copy(MDRVA position, LPCVOID src, SSIZE_T size) {

  assert(src);
  assert(size);

  // Ensure that the data will fit in the allocated space
  if (static_cast<size_t>(size + position) > size_)
    return false;

  // Seek and write the data
  DWORD NumberOfBytesWritten = NULL; 
  try {
	  /* This function myFileSeek used to know if file pointer has been moved 
	     requisite positions */

	  if (myFileSeek(hFile_, position, FILE_BEGIN) == static_cast<off_t>(position)) {
		  if (WriteFile(hFile_, src, size, &NumberOfBytesWritten, NULL)) {
				return true;
		  }
	  }
	  return false;
  }
  catch(...) {
	
  }
}
#endif

/* myFileSeek is a wrapper for SetFilePointer http://msdn.microsoft.com/en-us/library/windows/desktop/aa365541%28v=vs.85%29.aspx */
#ifdef _WIN32
__int64 MinidumpFileWriter::myFileSeek(HANDLE hf, __int64 distance, DWORD MoveMethod)
{
   LARGE_INTEGER li;

   li.QuadPart = distance;

   li.LowPart = SetFilePointer (hf, 
                                li.LowPart, 
                                &li.HighPart,  
                                MoveMethod);

   if (li.LowPart == INVALID_SET_FILE_POINTER && GetLastError() 
       != NO_ERROR)
   {
      li.QuadPart = -1;
   }

   return li.QuadPart;
}

#endif


bool UntypedMDRVA::Allocate(size_t size) {
  assert(size_ == 0);
  size_ = size;
  position_ = writer_->Allocate(size_);
  return position_ != MinidumpFileWriter::kInvalidMDRVA;
}

bool UntypedMDRVA::Copy(MDRVA pos, const void *src, size_t size) {

  assert(src);
  assert(size);
  assert(pos + size <= position_ + size_);
#ifndef _WIN32
  return writer_->Copy(pos, src, size);
#else
  return writer_->Copy(pos, src, size); /* Implement in Windows*/
#endif

}

}  // namespace google_breakpad
