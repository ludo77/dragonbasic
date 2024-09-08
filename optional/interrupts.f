{ -- MF/TIN INTERRUPT functions
  -- Original code by Jeff Massung, 2003 }

\ setup an interrupt event handler (keypad)
code-thumb onkey ( addr n1 n2 -- )
	r1 r2 r3 pop
	$4000132 r4 LITERAL
	
	\ write mask to REG_KEYCNT
	0 ## r2 cmp,
	__no_handler eq? b,
	\ enable IRQ only if there is a non-zero handler address
	$4000 r5 movi
	r5 r1 r1 add,
l: __no_handler

	0 ## r0 cmp,
	6 #offset eq? b,
	1 ## r5 r5 lsl,		\ $8000
	r5 r1 r1 add,

	r4 0@ r1 strh,
	
	\ write address of interrupt handler
	IWRAM_GLOBALS r0 LITERAL
	r0 INT_P1 #( r2 str,
	
	\ set key interrupt flag in IE
	$ce ## r4 add,		\ REGISTERS + $200
	r4 0@ r0 ldrh,
	
	\ if the address is zero then turn off the interrupt
	$1000 r5 movi
	r5 r0 bic,

	0 ## r2 cmp,
	4 #offset eq? b,
	r5 r0 orr,

	r4 0@ r0 strh,
	
	\ done
	r3 r0 mov,
	ret
end-code

\ setup an interrupt event handler (user timer)
code-thumb ontimer ( a -- )
	\ offset for timer registers
	$4000100 v0 LITERAL	\ REGISTERS + $100
	
	\ clear IRQ bit or set it for timer 2
	v0 $a #( v1 ldrh,
	$40 ## v2 mov,
	v2 v1 bic,
	\ test for no interrupt
	0 ## tos cmp,
	4 #offset eq? b,
	v2 v1 orr,
	v0 $a #( v1 strh,
	
	\ offset to interrupt registers
	$80 ## v0 add,
	$80 ## v0 add,
	
	\ write address of interrupt handler
	IWRAM_GLOBALS v2 LITERAL
	v2 INT_T2 #( tos str,
	
	\ set timer interrupt flag (or clear)
	$20 ## v2 mov,
	v0 0@ v1 ldrh,
	v2 v1 bic,
	\ test for no interrupt
	0 ## tos cmp,
	4 #offset eq? b,
	v2 v1 orr,
	v0 0@ v1 strh,
	
	\ done
	tos pop
	ret
end-code

\ setup an interrupt event handler (horizontal blank)
code-thumb onhblank ( a -- )
	REGISTERS v0 movi
	
	\ enable hblank bit in REG_DISPSTAT
	v0 4 #( v1 ldrh,
	$10 ## a mov,
	a v1 bic,

	0 ## tos cmp,
	4 #offset eq? b,
	a v1 orr,

	v0 4 #( v1 strh,
	
	\ offset to interrupt registers
	$200 a movi
	a v0 v0 add,
	
	\ write address of interrupt handler
	IWRAM_GLOBALS v2 LITERAL
	v2 INT_HB #( tos str,
	
	\ set hblank interrupt flag
	v0 0@ v1 ldrh,
	2 ## a mov,
	a v1 bic,

	0 ## tos cmp,
	4 #offset eq? b,
	a v1 orr,

	v0 0@ v1 strh,
	
	\ done
	tos pop
	ret
end-code

\ setup an interrupt event handler (vertical blank)
code-thumb onvblank ( a -- )
	REGISTERS v0 movi
	
	\ enable vblank bit in REG_DISPSTAT
	v0 4 #( v1 ldrh,
	8 ## a mov,
	a v1 bic,

	0 ## tos cmp,
	4 #offset eq? b,
	a v1 orr,

	v0 4 #( v1 strh,
	
	\ offset to interrupt registers
	$200 a movi
	a v0 v0 add,
	
	\ write address of interrupt handler
	IWRAM_GLOBALS v2 LITERAL
	v2 INT_VB #( tos str,
	
	\ set vblank interrupt flag
	v0 0@ v1 ldrh,
	1 ## a mov,
	a v1 bic,

	0 ## tos cmp,
	4 #offset eq? b,
	a v1 orr,

	v0 0@ v1 strh,
	
	\ done
	tos pop
	ret
end-code

\ setup an interrupt event handler (vertical count)
code-thumb onvcount ( a count -- )
	w pop
	
	\ address of registers
	REGISTERS v0 movi
	
	\ enable vcountbit in REG_DISPSTAT
	v0 4 #( v1 ldrh,
	$20 ## a mov,
	a v1 bic,

	0 ## w cmp,
	4 #offset eq? b,
	a v1 orr,

	8 ## tos tos lsl,
	tos v1 v1 add, \ count
	v0 4 #( v1 strh,
	
	\ offset to interrupt registers
	$200 a movi
	a v0 v0 add,
	
	\ write address of interrupt handler
	IWRAM_GLOBALS v2 LITERAL
	v2 INT_VC #( w str,
	
	\ set vblank interrupt flag
	v0 0@ tos ldrh,
	4 ## a mov,
	a tos bic,

	0 ## w cmp,
	4 #offset eq? b,
	a tos orr,

	v0 0@ tos strh,
	
	\ done
	tos pop
	ret
end-code
