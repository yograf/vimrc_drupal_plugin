" We never :set ft=drupal.  This filetype is always added to another, as in
" :set ft=php.drupal or :set ft=css.drupal.

" Syntastic settings, adapted from
" echodittolabs.org/drupal-coding-standards-vim-code-sniffer-syntastic-regex
if &ft =~ '\<php\>' && exists('loaded_syntastic_plugin') && executable('phpcs')
  let g:syntastic_phpcs_conf = ' --standard=Drupal'
	\ . ' --extensions=php,module,inc,install,test,profile,theme'
endif

" The tags file can be used for PHP omnicompletion even if $DRUPAL_ROOT == ''.
" If $DRUPAL_ROOT is set correctly, then the tags file can also be used for
" tag searches. Look for tags files in the project (module, theme, etc.)
" directory, the Drupal root directory, and in ../tagfiles/.
" TODO:  If we do not know which version of Drupal core, add no tags file or
" all?
let tags = []
if strlen(b:Drupal_info.INFO_FILE)
  let tags += [fnamemodify(b:Drupal_info.INFO_FILE, ':p:h') . '/tags']
endif
if strlen(b:Drupal_info.DRUPAL_ROOT)
  let tags += [fnamemodify(b:Drupal_info.DRUPAL_ROOT, ':p:h') . '/tags']
endif
if strlen(b:Drupal_info.CORE)
  let tagfile = 'drupal' . b:Drupal_info.CORE . '.tags'
  " <sfile>:p = .../vimrc/bundle/vim-plugin-for-drupal/ftplugin/drupal.vim
  let tags += [expand('<sfile>:p:h:h') . '/tagfiles/' . tagfile]
endif
for tagfile in tags
  " Bail out if the tags file has already been added.
  if stridx(&l:tags, tagfile) == -1
    " This is like :setlocal tags += ... but without having to escape special
    " characters.
    " :help :let-option
    let &l:tags .= ',' . tagfile
  endif
endfor

setl nojoinspaces            "No second space when joining lines that end in "."
setl autoindent              "Auto indent based on previous line
setl smartindent             "Smart autoindenting on new line
setl smarttab                "Respect space/tab settings
setl expandtab               "Tab key inserts spaces
setl tabstop=2               "Use two spaces for tabs
setl shiftwidth=2            "Use two spaces for auto-indent
setl textwidth=80            "Limit comment lines to 80 characters.
setl formatoptions-=t
setl formatoptions+=croql
"  -t:  Do not apply 'textwidth' to code.
"  +c:  Apply 'textwidth' to comments.
"  +r:  Continue comments after hitting <Enter> in Insert mode.
"  +o:  Continue comments after when using 'O' or 'o' to open a new line.
"  +q:  Format comments using q<motion>.
"  +l:  Do not break a comment line if it is long before you start.

" {{{ PHP specific settings.
" In ftdetect/drupal.vim we set ft=php.drupal.  This means that the settings
" here will come after those set by the PHP ftplugins.  In particular, we can
" override the 'comments' setting.

if &ft =~ '\<php\>'
  " In principle, PHP is case-insensitive, but Drupal coding standards pay
  " attention to case. This option affects searching in files and also tag
  " searches and code completion. If you want a case-insensitive search, start
  " the pattern with '\c'.
  setl noignorecase
  " Format comment blocks.  Just type / on a new line to close.
  " Recognize // (but not #) style comments.
  setl comments=sr:/**,m:*\ ,ex:*/,://
endif
" }}} PHP specific settings.

" The usual variable, b:did_ftplugin, is already set by the ftplugin for the
" primary filetype, so use a custom variable. The Syntastic and tags setting
" above are global, so check them each time we enter the buffer in case they
" have been changed.  Everything below is buffer-local.
if exists("b:did_drupal_ftplugin")  && exists("b:did_ftplugin") | finish | endif
let b:did_drupal_ftplugin = 1

augroup Drupal
  autocmd! BufEnter <buffer> call s:BufEnter()
augroup END

" {{{ @function s:BufEnter()
" There are some things that we *wish* were local to the buffer.  We stuff
" them into this function and call them from the autocommand above.
" - @var $DRUPAL_ROOT
"   Set this environment variable from b:Drupal_info.DRUPAL_ROOT.
" - SnipMate settings
let s:snip_path = expand('<sfile>:p:h:h') . '/snipmate/drupal'
function! s:BufEnter()
  if strlen(b:Drupal_info.DRUPAL_ROOT)
    let $DRUPAL_ROOT = b:Drupal_info.DRUPAL_ROOT
  endif
  if exists('*ExtractSnips')
    call ResetSnippets('drupal')
    " Load the version-independent snippets.
    let snip_path = s:snip_path . '/'
    for ft in split(&ft, '\.')
      call ExtractSnips(snip_path . ft, 'drupal')
      call ExtractSnipsFile(snip_path . ft . '.snippets', 'drupal')
    endfor
    " If we know the version of Drupal, add the coresponding snippets.
    if strlen(b:Drupal_info.CORE)
      let snip_path = s:snip_path . b:Drupal_info.CORE . '/'
      for ft in split(&ft, '\.')
	call ExtractSnips(snip_path . ft, 'drupal')
	call ExtractSnipsFile(snip_path . ft . '.snippets', 'drupal')
      endfor
    endif " strlen(b:Drupal_info.CORE)
  endif " exists('*ExtractSnips')
endfun
" }}} s:BufEnter()

if !exists('*s:OpenURL')

function s:OpenURL(base)
  let open = b:Drupal_info.OPEN_COMMAND
  if open == ''
    return
  endif
  " Get the word under the cursor.
  let func = expand('<cword>')
  " Some API sites let you specify which Drupal version you want.
  let core = strlen(b:Drupal_info.CORE) ? b:Drupal_info.CORE . '/' : ''
  " Custom processing for several API sites.
  if a:base == 'api.d.o'
    let url = 'http://api.drupal.org/api/search/' . core
  elseif a:base == 'hook'
    let url = 'http://api.drupal.org/api/search/' . core
    " Find the module or theme name and replace it with 'hook'.
    let root = expand('%:t:r')
    let func = substitute(func, '^' . root, 'hook', '')
  elseif a:base == 'drupalcontrib'
    let url = 'http://drupalcontrib.org/api/search/' . core
  else
    let url = a:base
    execute '!' . open . ' ' . a:base . func
  endif
  call system(open . ' ' . url . shellescape(func))
endfun

endif " !exists('*s:OpenURL')

" Add some menu items.

let s:options = {'root': 'Drupal', 'special': '<buffer>'}
if strlen(b:Drupal_info.OPEN_COMMAND)

  " Lookup the API docs for a drupal function under cursor.
  nmap <Plug>DrupalAPI :silent call <SID>OpenURL('api.d.o')<CR><C-L>
  call drupal#CreateMaps('n', 'Drupal API', '<LocalLeader>da',
	\ '<Plug>DrupalAPI', s:options)

  " Lookup the API docs for a drupal hook under cursor.
  nmap <Plug>DrupalHook :silent call <SID>OpenURL('hook')<CR><C-L>
  call drupal#CreateMaps('n', 'Drupal Hook', '<LocalLeader>dh',
	\ '<Plug>DrupalHook', s:options)

  " Lookup the API docs for a contrib function under cursor.
  nmap <Plug>DrupalContribAPI :silent call <SID>OpenURL('drupalcontrib')<CR><C-L>
  call drupal#CreateMaps('n', 'Drupal contrib', '<LocalLeader>dc',
	\ '<Plug>DrupalContribAPI', s:options)

  " Lookup the API docs for a drush function under cursor.
  nmap <Plug>DrushAPI :silent call <SID>OpenURL("http://api.drush.ws/api/function/")<CR><C-L>
  call drupal#CreateMaps('n', 'Drush API', '<LocalLeader>dda',
	\ '<Plug>DrushAPI', s:options)
endif

" Get the value of the drupal variable under cursor.
nnoremap <buffer> <LocalLeader>dv :execute "!drush vget ".shellescape(expand("<cword>"), 1)<CR>
  call drupal#CreateMaps('n', 'variable_get', '<LocalLeader>dv',
	\ ':execute "!drush vget ".shellescape(expand("<cword>"), 1)<CR>',
	\ s:options)
