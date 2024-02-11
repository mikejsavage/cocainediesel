#pragma once

struct SourceLocation {
	const char * file;
	int line;
	const char * function;
};

constexpr SourceLocation CurrentSourceLocation( const char * file = __builtin_FILE(), int line = __builtin_LINE(), const char * function = __builtin_FUNCTION() ) {
	return {
		.file = file,
		.line = line,
		.function = function,
	};
}
