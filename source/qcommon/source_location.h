#pragma once

struct SourceLocation {
	const char * file;
	int line;
	const char * function;
};

constexpr SourceLocation CurrentSourceLocation( const char * file_ = __builtin_FILE(), int line_ = __builtin_LINE(), const char * function_ = __builtin_FUNCTION() ) {
	return {
		.file = file_,
		.line = line_,
		.function = function_,
	};
}
