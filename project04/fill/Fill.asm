// This file is part of www.nand2tetris.org
// and the book "The Elements of Computing Systems"
// by Nisan and Schocken, MIT Press.
// File name: projects/4/Fill.asm

// Runs an infinite loop that listens to the keyboard input.
// When a key is pressed (any key), the program blackens the screen,
// i.e. writes "black" in every pixel. When no key is pressed,
// the screen should be cleared.

(LOOP)

  // if key pressed: goto BLK
  @KBD
  D=M
  @BLK
  D;JNE

  // set fill to white
  @fill
  M=0

  // goto DRAW
  @DRAW
  0;JMP

  // set fill to black
  (BLK)
    @fill
    M=-1

  (DRAW)
    @SCREEN
    D=A
    @idx
    M=D
    @8192
    D=D+A
    @end
    M=D

    (DRAWLOOP)
      @fill
      D=M
      @idx
      A=M
      M=D
      @idx
      D=M+1
      M=D

      @end
      D=D-M

      @DRAWLOOP
      D;JNE

  @LOOP
  0;JMP

(END)
@END
0;JMP
