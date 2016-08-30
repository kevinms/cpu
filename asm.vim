" Vim syntax file
" Language:	My Assembly
" Maintainer:	Kevin M. Smith
" Last Change:	2015 Oct 20

" Quit when a (custom) syntax file was already loaded
if exists("b:current_syntax")
  finish
endif

let s:cpo_save = &cpo
set cpo&vim

syn case ignore

syn keyword asmType nop
syn keyword asmType add sub adc sbc mul div
syn keyword asmType ldw ldb stw stb
syn keyword asmType mov
syn keyword asmType and or xor nor lsl lsr
syn keyword asmType cmp jmp jz jnz jl jg
syn keyword asmType in out
syn keyword asmType die

syn keyword asmType w b

" syn match asmLabel		"[a-z_][a-z0-9_]*:"he=e-1
syn match asmLabel		"\.[a-z_][a-z0-9_]*"
syn match asmIdentifier		"[a-z_][a-z0-9_]*"

" Various #'s as defined by GAS ref manual sec 3.6.2.1
" Technically, the first decNumber def is actually octal,
" since the value of 0-7 octal is the same as 0-7 decimal,
" I (Kevin) prefer to map it as decimal:
syn match decNumber		"0\+[1-7]\=[\t\n$,; ]"
syn match decNumber		"[1-9]\d*"
syn match octNumber		"0[0-7][0-7]\+"
syn match hexNumber		"0[xX][0-9a-fA-F]\+"
syn match binNumber		"0[bB][0-1]*"

syn keyword asmTodo		contained TODO


" GAS supports one type of multi line comments:
syn region asmComment		start="/\*" end="\*/" contains=asmTodo

" GAS (undocumentedly?) supports C++ style comments. Unlike in C/C++ however,
" a backslash ending a C++ style comment does not extend the comment to the
" next line (hence the syntax region does not define 'skip="\\$"')
syn region asmComment		start="//" end="$" keepend contains=asmTodo

" Line comment characters depend on the target architecture and command line
" options and some comments may double as logical line number directives or
" preprocessor commands. This situation is described at
" http://sourceware.org/binutils/docs-2.22/as/Comments.html
" Some line comment characters have other meanings for other targets. For
" example, .type directives may use the `@' character which is also an ARM
" comment marker.
" As a compromise to accommodate what I arbitrarily assume to be the most
" frequently used features of the most popular architectures (and also the
" non-GNU assembly languages that use this syntax file because their asm files
" are also named *.asm), the following are used as line comment characters:
syn match asmComment		"[#;!|].*" contains=asmTodo

" Side effects of this include:
" - When `;' is used to separate statements on the same line (many targets
"   support this), all statements except the first get highlighted as
"   comments. As a remedy, remove `;' from the above.
" - ARM comments are not highlighted correctly. For ARM, uncomment the
"   following two lines and comment the one above.
"syn match asmComment		"@.*" contains=asmTodo
"syn match asmComment		"^#.*" contains=asmTodo

" Advanced users of specific architectures will probably want to change the
" comment highlighting or use a specific, more comprehensive syntax file.

syn match asmDirective		"[a-z][a-z]\+"

" string contstants and special characters within them
syn match  asmSpecial contained "\\['\\]"
syn region asmString start=+'+ skip=+\\"+ end=+'+ contains=asmSpecial

syn case match

hi def link hexNumber	Number
hi def link decNumber	Number
hi def link octNumber	Number
hi def link binNumber	Number

hi def link asmTodo	Todo
hi def link asmType	Conditional
hi def link asmComment	Comment
hi def link asmLabel	Label
hi def link asmIdentifier	Identifier
hi def link asmDirective	Identifier
hi def link asmIdentifier	Conditional
hi def link asmDirective	Conditional

hi def link asmSpecial		Special
hi def link asmString		String

let b:current_syntax = "asm"

let &cpo = s:cpo_save
unlet s:cpo_save

" vim: ts=8
