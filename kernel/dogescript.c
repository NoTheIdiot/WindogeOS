#include <system.h>
#include <dogeio.h>
#include <string.h>
#include <math.h>

// this is a scripting language i decided to make for the operating system
// to temporaily solve the no programs problem, or it becomes permanant and
// programs and dogescript will live together

void system_dogescript_interprate(const char* line_string, int line) {
	if (string_startswith(line_string, "print")) {
    	dogeio_text_println(line_string + 6);
    } else {
        dogeio_text_print  ("error: function not found at line ");
        dogeio_text_println(line);
    }
}

void system_dogescript(char** array) {
	// pass
}
#include <dogeio.h>
#include <system.h>