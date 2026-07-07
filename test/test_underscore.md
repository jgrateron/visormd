# Underscore formatting tests (_italic_ and __bold__)

## Basic italic
*This text will be italic with asterisks*
_This will also be italic with underscores_

## Basic bold
**This text will be bold with asterisks**
__This will also be bold with underscores__

## Mixed in one line
Mixed _italic_ and **bold** in one line
Mixed *italic* and __bold__ in one line

## Nested combinations
_Italic wrapping **bold** inside_
*Italic wrapping __bold__ inside*
__Bold wrapping *italic* inside__
**Bold wrapping _italic_ inside**

## Triple for bold+italic
___triple underscore bold+italic___
***triple asterisk bold+italic***
__*bold italic*__ combined
_**bold italic**_ combined

## snake_case preservation
This_is_a_snake_case_identifier should stay as is
normal_text_with_underscores inside words

## Edge cases
single_underscore_at_end_of_long_word_is_literal
_bold_inside_underscores_does_not_break_them
