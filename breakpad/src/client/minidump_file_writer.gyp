{
	'targets': [
	  {
		'target_name': 'minidump_file_writer_unittest',
		'type': 'executable',
		'sources': [
		   'minidump_file_writer.h',
		   'minidump_file_writer-inl.h',
		   'minidump_file_writer.cc',
		   'minidump_file_writer_unittest.cc',
		   '<(DEPTH)/common/convert_UTF.c',
		   '<(DEPTH)/common/string_conversion.cc'
		],
		'include_dirs': [
		   '..',
			],
	  }
	]
}