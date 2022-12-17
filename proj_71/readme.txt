Projeto de RC 2022/2023
- Francisco Lopes 99220
- Nuno Martins 99292
------------------------------------

To compile just run "make" inside proj_71 directory
- Player: use ./player [-n GSIP] [-p GSport]
- Server: use ./GS word_eng.txt [-p GSport]

To clean the directory use "make clean" (WARNING: this also removes all the .txt files
inside the <games> and <scores> folders).

------------------------------------

NOTES:
- Client/Player is fully functional and working correctly with tejo server with all UDP
and TCP commands
- Server is fully functional ONLY for UDP commands. Due to time constraints we couldn't
properly implement the TCP commands and test it, so we removed them altogether.