#include <stdio.h>
#include <stdint.h>


int main(int argc, char* argv[]) {
	uint64_t fileCount = 0;
	for (int i = 1; i < argc; ++i) {
		if (argv[i][0] != '-') {
			fileCount++;
			continue;
		} else {
			printf("warning: flag %s ignored\n", argv[i]);
		}
	}

	if (fileCount == 0) {
		printf("aicc: error: no input files\n");

		return 1;
	}


	return 0;
}

