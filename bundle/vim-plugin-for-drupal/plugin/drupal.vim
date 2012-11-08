" Drupal settings for highlighting and indenting:  see
" :help ft-php-syntax and comments in $VIMRUNTIME/indent/php.vim .
let php_htmlInStrings = 1   "Syntax highlight for HTML inside PHP strings
let php_parent_error_open = 1 "Display error for unmatch brackets
let PHP_autoformatcomment = 0
let PHP_removeCRwhenUnix = 1

" {{{
" Everything from here on assumes that autocommands are available.
" }}}
if !has("autocmd")
  finish
endif

augroup Drupal
  " Remove ALL autocommands for the Drupal group.
  autocmd!
  " s:DrupalInit() will create the buffer-local Dictionary b:Drupal_info
  " containing useful information for ftplugins and syntax files.  It will
  " also add drupal as a secondary filetype.  This will load the ftplugins and
  " syntax files drupal.vim after the usual ones.
  autocmd FileType php,css,javascript,drini call s:DrupalInit()

  " Highlight trailing whitespace.
  autocmd BufWinEnter * call s:ToggleWhitespaceMatch('BufWinEnter')
  autocmd BufWinLeave * call s:ToggleWhitespaceMatch('BufWinLeave')
  autocmd InsertEnter * call s:ToggleWhitespaceMatch('InsertEnter')
  autocmd InsertLeave * call s:ToggleWhitespaceMatch('InsertLeave')
augroup END
highlight default link drupalExtraWhitespace Error

" Adapted from http://vim.wikia.com/wiki/Highlight_unwanted_spaces
function! s:ToggleWhitespaceMatch(event)
  " Bail out unless the filetype is php.drupal, css.drupal, ...
  if &ft !~ '\<drupal\>'
    return
  endif
  if a:event == 'BufWinEnter'
    let w:whitespace_match_number = matchadd('drupalExtraWhitespace', '\s\+$')
    return
  endif
  if !exists('w:whitespace_match_number')
    return
  endif
  call matchdelete(w:whitespace_match_number)
  if a:event == 'BufWinLeave'
    unlet w:whitespace_match_number
  elseif a:event == 'InsertEnter'
    call matchadd('drupalExtraWhitespace', '\s\+\%#\@<!$', 10, w:whitespace_match_number)
  elseif a:event == 'InsertLeave'
    call matchadd('drupalExtraWhitespace', '\s\+$', 10, w:whitespace_match_number)
  endif
endfunction

" Borrowed from autoload/pathogen.vim:
let s:slash = !exists("+shellslash") || &shellslash ? '/' : '\'

" {{{ @function s:DrupalInit()
" Save some information in the buffer-local Dictionary b:Drupal_info for use
" by ftplugin and syntax scripts.  The keys are
" - DRUPAL_ROOT
"   path to the Drupal root
" - INFO_FILE
"   path to the .info file of the containing module, theme, etc.
" - TYPE
"   'module' or 'theme' or 'make'
" - OPEN_COMMAND
"   'open' or 'xdg-open' or 'cmd /c start', depending on the OS
" In all cases, the values will be '' if we cannot make a reasonable guess.
" {{{
function! s:DrupalInit()
  " Expect something like /var/www/drupal-7.9/sites/all/modules/ctools
  let path = expand('%:p')
  let directory = fnamemodify(path, ':h')
  let info = {'DRUPAL_ROOT': s:DrupalRoot(directory),
	\ 'INFO_FILE': s:InfoPath(directory)}
  let info.OPEN_COMMAND = s:OpenCommand()
  let info.TYPE = s:IniType(info.INFO_FILE)
  let info.CORE = s:CoreVersion(info.INFO_FILE)
  " If we found only one of CORE and DRUPAL_ROOT, use it to get the other.
  if info.CORE == '' && info.DRUPAL_ROOT != ''
    let INFO_FILE = info.DRUPAL_ROOT . '/modules/system/system.info'
    if filereadable(INFO_FILE)
      let info.CORE = s:CoreVersion(INFO_FILE)
    else
      let INFO_FILE = info.DRUPAL_ROOT . '/core/modules/system/system.info'
      if filereadable(INFO_FILE)
	let info.CORE = s:CoreVersion(INFO_FILE)
      endif
    endif
  elseif info.DRUPAL_ROOT == '' && info.CORE != ''  && exists('g:Drupal_dirs')
    let info.DRUPAL_ROOT = get(g:Drupal_dirs, info.CORE, '')
  endif

  " @var b:Drupal_info
  let b:Drupal_info = info
  " TODO:  If we are not inside a Drupal directory, maybe skip this.  Wait
  " until someone complains that we are munging his non-Drupal php files.
  set ft+=.drupal
endfun
" }}} }}}

" {{{ @function s:DrupalRoot()
" Try to guess which part of the path is the Drupal root directory.
"
" @param path
"   A string representing a system path.
"
" @return
"   A string representing the Drupal root, '' if not found.
" {{{
function! s:DrupalRoot(path)
  " If all the markers are found, assume the directory is the Drupal root.
  " See update_verify_update_archive() for official markers.
  let markers = {}
  let markers.7 = [['index.php'], ['update.php']]
  call add(markers.7, ['includes', 'bootstrap.inc'])
  call add(markers.7, ['modules', 'node', 'node.module'])
  call add(markers.7, ['modules', 'system', 'system.module'])
  let markers.8 = [['index.php']]
  call add(markers.8, ['core', 'update.php'])
  call add(markers.8, ['core', 'includes', 'bootstrap.inc'])
  call add(markers.8, ['core', 'modules', 'node', 'node.module'])
  call add(markers.8, ['core', 'modules', 'system', 'system.module'])
  for marker_list in values(markers)
    call map(marker_list, 'join(v:val, s:slash)')
  endfor
  let g:marker_list = marker_list

  " On *nix, start with '', but on Windows typically start with 'C:'.
  let path_components = split(a:path, s:slash, 1)
  let droot = remove(path_components, 0)

  for part in path_components
    let droot .= s:slash . part
    for marker_list in values(markers)
      let is_drupal_root = 1
      for marker in marker_list
	" Since globpath() is built in to vim, this should be fast.
	if globpath(droot, marker) == ''
	  let is_drupal_root = 0
	  break
	endif
      endfor " marker
      " If all the markers are there, then this looks like a Drupal root.
      if is_drupal_root
	return droot
      endif
    endfor " marker_list
  endfor " part
  return ''
endfun
" }}} }}}

" {{{ @function s:InfoPath()
" Try to find the .info file of the module, theme, etc. containing a path.
"
" @param path
"   A string representing a system path.
"
" @return
"   A string representing the path of the .info file, '' if not found.
" {{{
function! s:InfoPath(path)
  let dir = a:path
  while dir =~ '\' . s:slash
    let infopath = glob(dir . s:slash . '*.{info,make,build}')
    if strlen(infopath)
      return infopath
    endif
    " No luck yet, so go up one directory.
    let dir = substitute(dir, '\' . s:slash . '[^\' . s:slash . ']*$', '', '')
  endwhile
  return ''
endfun
" }}} }}}

" Return a string that can be used to open URL's (and other things).
" Usage:
" let open = s:OpenCommand()
" if strlen(open) | execute '!' . open . ' http://example.com' | endif
" See http://www.dwheeler.com/essays/open-files-urls.html
function! s:OpenCommand()
if has('macunix') && executable('open')
  return 'open'
endif
if has('win32unix') && executable('cygstart')
  return 'cygstart'
endif
if has('unix') && executable('xdg-open')
  return 'xdg-open'
endif
if (has('win32') || has('win64')) && executable('cmd')
  return 'cmd /c start'
endif
  return ''
endfun

" {{{ @function s:CoreVersion(info_path)
" Find the version of Drupal core by parsing the .info file.
"
" @param info_path
"   A string representing the path to the .info file.
"
" @return
"   A numeric string representing the Drupal core version.
" {{{
function! s:CoreVersion(info_path)
  " Find the Drupal core version.
  if a:info_path == '' || !filereadable(a:info_path)
    return ''
  endif
  let lines = readfile(a:info_path, '', 500)
  let core_re = '^\s*core\s*=\s*\zs\d\+\ze\.x\s*$'
  let core_line = matchstr(lines, core_re)
  return matchstr(core_line, core_re)
endfun
" }}} }}}

" {{{ @function s:IniType(info_path)
" Find the type (module, theme, make) by parsing the path.
"
" @param info_path
"   A string representing the path to the .info file.
"
" @return
"   A string:  module, theme, make
" {{{
" TODO:  How do we recognize a Profiler .info file?
function! s:IniType(info_path)
  let ext = fnamemodify(a:info_path, ':e')
  if ext == 'make' || ext == 'build'
    return 'make'
  else
    " If the extension is not 'info' at this point, I do not know how we got
    " here.
    let m_index = strridx(a:info_path, s:slash . 'modules' . s:slash)
    let t_index = strridx(a:info_path, s:slash . 'themes' . s:slash)
    " If neither matches, try a case-insensitive search.
    if m_index == -1 && t_index == -1
      let m_index = matchend(a:info_path, '\c.*\' . s:slash . 'modules\' . s:slash)
      let t_index = matchend(a:info_path, '\c.*\' . s:slash . 'themes\' . s:slash)
    endif
    if m_index > t_index
      return 'module'
    elseif m_index < t_index
      return 'theme'
    endif
    " We are not inside a themes/ directory, nor a modules/ directory.  Do not
    " guess.
    return ''
  endif
endfun
" }}} }}}

" {{{
" :Drush <subcommand> executes "Drush <subcommand>" and puts the output in a
" new window. Command-line completion uses the output of "drush --sort --pipe"
" unless the current argument starts with "@", in which case it uses the
" output of "drush site-alias".
command! -nargs=* -complete=custom,s:DrushComplete Drush call s:Drush(<q-args>)
" {{{
function! s:Drush(command) abort
  " Open a new window. It is OK to quit without saving, and :w does nothing.
  new
  setlocal buftype=nofile bufhidden=hide noswapfile
  " Do not wrap long lines and try to handle ANSI escape sequences.
  setl nowrap
  " For now, just use the --nocolor option.
  " if exists(":AnsiEsc") == 2
    " AnsiEsc
  " endif
  " Execute the command and grab the output. Clean it up.
  " TODO: Does the clean-up work on other OS's?
  let commandline = 'drush --nocolor ' . a:command
  let shortcommand = 'drush ' . a:command
  " Change the status line to list the command instead of '[Scratch]'.
  let &l:statusline = '%<[' . shortcommand . '] %h%m%r%=%-14.(%l,%c%V%) %P'
  let out = system(commandline)
  let out = substitute(out, '\s*\r', '', 'g')
  " Add the command and output to our new scratch window.
  put = '$ ' . shortcommand
  put = '==' . substitute(shortcommand, '.', '=', 'g')
  put = out
  " Delete the blank line at the top and stay there.
  1d
endfun
" }}}
" On Windows, shelling out is slow, so let's cache the results.
let s:drush_completions = {'command': '', 'alias': ''}
function! s:DrushComplete(ArgLead, CmdLine, CursorPos) abort" {{{
  let options = ''
  if a:ArgLead =~ '@\S*$'
    if s:drush_completions.alias == ''
      let s:drush_completions.alias = system('drush site-alias')
    endif
    let options = s:drush_completions.alias
  else
    if s:drush_completions.command == ''
      let commands = system('drush --sort --pipe')
      let s:drush_completions.command = substitute(commands, ' \+', '\n', 'g')
    endif
    let options = s:drush_completions.command
  endif
  return options
endfun
" }}} }}}

" {{{ @
function! s:SetDrupalRoot()
  let dir = input('Drupal root directory: ', b:Drupal_info.DRUPAL_ROOT, 'file')
  let b:Drupal_info.DRUPAL_ROOT = expand(substitute(dir, '[/\\]$', '', ''))
  if strlen(dir)
    let INFO_FILE = b:Drupal_info.DRUPAL_ROOT . '/modules/system/system.info'
    if filereadable(INFO_FILE)
      let b:Drupal_info.CORE = s:CoreVersion(INFO_FILE)
    endif
  endif
endfun
" }}}
nmap <Plug>DrupalSetRoot :call <SID>SetDrupalRoot()<CR>
let s:options = {'root': 'Drupal.Configure', 'weight': '900.'}
call drupal#CreateMaps('n', 'Set Drupal root', '', '<Plug>DrupalSetRoot', s:options)
call drupal#CreateMaps('n', 'Show Drupal info', '', ':echo b:Drupal_info<CR>', s:options)
