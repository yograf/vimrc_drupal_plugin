<?php

/**
 * @file
 *   Drush tools to support the vimrc project.
 */

/**
 * Implements hook_drush_command().
 */
function vimrc_drush_command() {
  $items = array();

  $items['vimrc-install'] = array(
    'description' => 'Install the vim plugin for Drupal (vimrc project).',
    'arguments' => array(
    ),
    'options' => array(
      'vim-command' => 'Command used to invoke vim (defaults to vim).',
      'vimfiles' => 'Path to vimfiles directory (defaults to $HOME/.vim, or $HOME/vimfiles on Windows).',
      'vimrc' => 'Path to vimrc file (defaults to $HOME/.vimrc, or $HOME/_vimrc on Windows).',
    ),
    'examples' => array(
      'drush vimrc-install --vim-command=/home/drupaluser/bin/vim'
      => 'Install the plugin using the executable at /home/drupaluser/bin/vim.',
      'drush vimrc-install --vimfiles=/usr/share/vim/vimcurrent --vimrc=/usr/share/vim/vimrc'
      => 'Install the plugin system-wide. The example paths are suitable for some Linux distributions.',
    ),
    'aliases' => array(),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );

  $items['vimrc-tag-gen'] = array(
    'description' => 'Generate a tags file for use by vim and other programs.',
    'arguments' => array(
      'path' => 'System path to a file or directory (e.g. your Drupal distribution or a module folder). Defaults to the current directory.',
    ),
    'options' => array(
      'tag-file' => 'Name of the output file (defaults to "tags")',
      'ctags' => 'Path to the exuberant ctags executable (defaults to "ctags")',
      'include-js' => 'Include tags for .js files (defaults to "yes")',
      'include-test' => 'Include tags for .test files (defaults to "yes")',
    ),
    'examples' => array(
      'drush vimrc-tag-gen $HOME/Sites/drupal-7 --tag-file=drupal7.tags'
      => 'Generate a tags file for your distribution of Drupal 7 and save it as drupal7.tags in the current directory.',
      'drush tags sites/all/modules/ctools --ctags=/usr/local/bin/ctags'
      => 'Generate a tags file for the ctools module using a specific ctage executable and save it as tags in the current directory.',
    ),
    'aliases' => array('tags'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUSH,
  );

  return $items;
}

/**
 * Implements drush_hook_COMMAND_validate().
 */
function drush_vimrc_install_validate() {
  // Bail out if vim is not executable or is not the right version.
  $vim = drush_get_option('vim-command', 'vim');
  $vim = escapeshellcmd($vim);
  $error = '';
  // This works only if $vim is a full path.
  // if (!is_executable($vim)) {
  //   $error = dt('The command %vim is not executable.', array('%vim' => $vim));
  // }
  // else {
  exec("$vim --version", $output, $retval);
  if ($retval) {
    $error = dt('The command %vim exited with an error.',
      array('%vim' => $vim));
  }
  elseif (!preg_match('/^VIM - Vi IMproved (\d+)/', $output[0], $matches)) {
    $error = dt('The command %vim is not Vi IMproved (vim).',
      array('%vim' => $vim));
  }
  elseif ((int) $matches[1] < 7) {
    $error = dt('The vimrc project requires vim 7.0 or later (7.3+ recommended).',
      array('%vim' => $vim));
  }
  // }
  if ($error) {
    $error .= "\n" . dt('See !url for how to install vim.',
      array('!url' => 'http://www.vim.org/download.php'));
    return drush_set_error('DRUSH_APPLICATION_ERROR', $error);
  }

  // Check that we can write to the vimrc file. If pathogen is not installed,
  // also check the vimfiles directory.
  $vimrc = drush_get_option('vimrc', _vimrc_vimrc_default());
  if (!is_file($vimrc) && !touch($vimrc)) {
    $error = dt('Cannot create the vimrc file %vimrc. Specify another file with --vimrc.',
      array('%vimrc' => $vimrc));
  }
  elseif (!is_writable($vimrc)) {
    $error = dt('Installation requires write access to the vimrc file %vimrc. Specify another file with --vimrc.',
      array('%vimrc' => $vimrc));
  }
  elseif (!_vimrc_check_pathogen($vim)) {
    // Need to install pathogen.
    $vimfiles = drush_get_option('vimfiles', _vimrc_vimfiles_default());
    if (!is_dir($vimfiles) && !mkdir($vimfiles, 0777, TRUE)) {
      $error = dt('Cannot create the vimfiles directory %vimfiles. Specify another directory with --vimfiles.',
	array('%vimfiles' => $vimfiles));
    }
    elseif (!is_writable($vimfiles)) {
      $error = dt('Installation requires write access to the vimfiles directory %vimfiles. Specify another directory with --vimfiles.',
	array('%vimfiles' => $vimfiles));
    }
  }
  if ($error) {
    return drush_set_error('DRUSH_APPLICATION_ERROR', $error);
  }
}

/**
 * Return the default vimfiles directory, depending on the OS.
 */
function _vimrc_vimfiles_default() {
    return $_ENV['HOME'] . (drush_is_windows() ?  '/vimfiles' : '/.vim');
}

/**
 * Return the default vimrc file, depending on the OS.
 */
function _vimrc_vimrc_default() {
    return $_ENV['HOME'] . (drush_is_windows() ?  '/_vimrc' : '/.vimrc');
}

/**
 * Check whether the pathogen plugin is installed.
 */
function _vimrc_check_pathogen($vim) {
  $lines = vimrc_vim_command($vim, '2verbose runtime autoload/pathogen.vim');
  if ($lines === FALSE) {
    return drush_set_error('DRUSH_APPLICATION_ERROR',
      dt('Error when calling vim to check for pathogen.'));
  }
  $message = array_pop($lines);
  return preg_match('#^finished sourcing .*autoload/pathogen.vim$#', $message);
}

/**
 * Run a vim (ex) command and return the output.
 *
 * The calling function is reponsible for validating that $vim is executable.
 *
 * Example: $versionstring = vimrc_vim_command('/usr/local/bin/vim', 'version')
 *
 * @param String $vim
 *   The name of the vim executable.
 * @param String $command
 *   The vim command to execute.
 * @return Array
 *   The output of $command as an array of strings or FALSE on error.
 */
function vimrc_vim_command($vim, $command) {
  // TODO:  Does this work on Windows?
  $escapedcommand = escapeshellcmd($command);
  $vimlog = drush_tempnam('vimrc');
  $infile = drush_save_data_to_temp_file("$escapedcommand\nq\n");
  // $lines = exec("echo $escapedcommand | $vim -e -s -V0$vimlog", $output, $retval);
  exec("$vim -e -s -V0$vimlog < $infile", $output, $retval);
  if ($retval > 1) {
    return FALSE;
  }
  // drush_shell_exec('$vim -e -s -V0%s < %s', $vimlog, $infile);
  $lines = file($vimlog);
  // Remove empty line and 'Entering Ex mode' line.
  array_shift($lines);
  array_shift($lines);
  // Remove 'empty buffer' message and empty line.
  array_pop($lines);
  array_pop($lines);
  return $lines;
}

/**
 * Implements drush_hook_COMMAND().
 */
function drush_vimrc_install($path = '.') {
  $vim = drush_get_option('vim-command', 'vim');
  $vim = escapeshellcmd($vim);
  $vimrc = drush_get_option('vimrc', _vimrc_vimrc_default());
  $vimfiles = drush_get_option('vimfiles', _vimrc_vimfiles_default());

  if (!_vimrc_check_pathogen($vim)) {
    // Pathogen is not installed, but the validate function already checked
    // that we can write to the vimfiles directory. Start by downloading
    // pathogen.  See https://github.com/tpope/vim-pathogen.
    drush_log("Creating directory $vimfiles/autoload.");
    mkdir("$vimfiles/autoload");
    $url =
      'https://raw.github.com/tpope/vim-pathogen/master/autoload/pathogen.vim';
    drush_log("Downloading pathogen from $url.");
    $pathogen = file_get_contents($url);
    drush_log("Saving it to $vimfiles/autoload/pathogen.vim.");
    file_put_contents("$vimfiles/autoload/pathogen.vim", $pathogen);
  }

  // Now, add a line to the vimrc file. If it is empty, start with the
  // standard one distributed with this script.
  $runcommands = file_get_contents($vimrc);
  $bundle = realpath('bundle');
  $call_pathogen = "call pathogen#infect('$bundle')";
  if (strpos($runcommands, $call_pathogen) === FALSE) {
    if (empty($runcommands)) {
      $runcommands = file_get_contents('examples/vimrc_standard.vim');
    }
    $comment = '" Following line added by drush vimrc-install on ' . date('r');
    drush_log("Rewriting vimrc file $vimrc.");
    file_put_contents($vimrc, "$comment\n$call_pathogen\n\n$runcommands");
  }

  return;
}

/**
 * Implements drush_hook_COMMAND_validate().
 */
function drush_vimrc_vimrc_tag_gen_validate() {
  // Bail out if ctags is not executable or is not the right version.
  $ctags = drush_get_option('ctags', 'ctags');
  $ctags = escapeshellcmd($ctags);
  exec("$ctags --version", $output, $retval);
  if ($retval || !preg_match('/^Exuberant Ctags/', $output[0])) {
    return drush_set_error('DRUSH_APPLICATION_ERROR', dt('Make sure that Exuberant Ctags is installed (http://ctags.sourceforge.net/) and specify the path with (for example) --ctags=/usr/local/bin/ctags'));
  }
  // Check that the tags file is writable.
  $tag_file = drush_get_option('tag-file', 'tags');
  $tag_file = realpath($tag_file);
  $path = file_exists($tag_file) ? $tag_file : dirname($tag_file);
  if (!is_writable($path)) {
    return drush_set_error('DRUSH_PERM_ERROR', dt('Cannot write the tags file. Specify another path with --tag-file=path/to/tags'));
  }
  // Check that the boolean flags are "yes" or "no".
  $flags = array('include-js', 'include-test');
  foreach ($flags as $flag) {
    $option = drush_get_option($flag, 'yes');
    if ($option != 'yes' && $option != 'no') {
      // TODO: Find a better error code.
      return drush_set_error('DRUSH_UNKNOWN_OPTION', dt('The --include-* options should be specified as "yes" or "no".'));
    }
  }
}

/**
 * Implements drush_hook_COMMAND().
 */
function drush_vimrc_vimrc_tag_gen($path = '.') {
  // Get the option values.  This duplicates a few lines of the validate
  // function. :-(
  $ctags = drush_get_option('ctags', 'ctags');
  $ctags = escapeshellcmd($ctags);
  $tag_file = drush_get_option('tag-file', 'tags');
  $tag_file = realpath($tag_file);
  $include_js = drush_get_option('include-js', 'yes');
  $include_test = drush_get_option('include-test', 'yes');
  // Figure out which options to use.
  $langmap = 'php:+.module.install.inc.profile.theme.engine';
  if ($include_test == 'yes') {
    $langmap .= '.test';
  }
  if ($include_js != 'yes') {
    $langmap .= ',javascript:';
  }
  $options = "-R --totals --extra=+f --langmap=$langmap --php-kinds=-v";
  // Run ctags, generating $tag_file.
  print dt('Some warning messages from ctags are normal.') . "\n";
  exec("$ctags -f $tag_file $options $path");
  // Read the file into an array.
  $lines = file($tag_file);
  $handle = fopen($tag_file, 'w');
  // Process the lines and rewrite the file.
  foreach ($lines as $line) {
    // Skip anything that looks like a comment.
    if (preg_match('#/\^\s*(\\\\/\\\\/|/\*|\*|\'\')#', $line) == 1) {
      continue;
    }
    // Skip anything that looks like 'key' => 'value'.
    if (preg_match('/[\'"]#?\w+[\'"]\s*=>/', $line) == 1) {
      continue;
    }
    // Skip anything that looks like $output .= 'lotsa text'.
    if (preg_match('#/\^\s*\$\w+\s*\.?\=#', $line) == 1) {
      continue;
    }
    // Replace the path (but not any trailing '/') with '$DRUPAL_ROOT'.
    $shortpath = preg_replace('#/$#', '', $path);
    $line = str_replace($shortpath, '$DRUPAL_ROOT', $line);
    fwrite($handle, $line);
  }
  fclose($handle);
}
