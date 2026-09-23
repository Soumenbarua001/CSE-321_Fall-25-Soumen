CSE 321: Operating Systems
SimpleFS Lab Term Project - Student Starter Files

FILES
1. simplefs.h          - fixed constants and structures; do not modify unless instructed.
2. simplefs_builder.c  - complete TODO 1 through TODO 6.
3. simplefs_adder.c    - complete TODO 1 through TODO 11.
4. test1.txt, test2.txt, test3.txt - simple sample input files.

COMPILE
gcc -Wall -Wextra -std=c11 simplefs_builder.c -o simplefs_builder
gcc -Wall -Wextra -std=c11 simplefs_adder.c -o simplefs_adder

RUN
./simplefs_builder --image disk.img
./simplefs_adder --input disk.img --file test1.txt

IMPORTANT
- Follow the official project specification for the exact layout and expected behavior.
- Source files must be in the current working directory.
- Maximum regular-file size is 12288 bytes.
- Implement only the clearly marked TODO sections.
