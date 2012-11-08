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
      'sudo drush vimrc-install --vimfiles=/usr/share/vim/vimcurrent --vimrc=/usr/share/vim/vimrc'
      => 'Install the plugin system-wide. The example paths are suitable for some Linux distributions.',
    ),
    'aliases' => array(),
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
  $infile = drush_save_data_to_temp_file("$command\nqa!\n");
  $vimlog = drush_tempnam('vimrc');
  // $lines = exec("echo $escapedcommand | $vim -e -s -V0$vimlog", $output, $retval);
  exec("$vim -e -s -N -V0$vimlog < $infile", $output, $retval);
  if ($retval > 1) {
    return FALSE;
  }
  $lines = file($vimlog);
  // Remove empty line and 'Entering Ex mode' line.
  unset($lines[0], $lines[1]);
  // Remove 'empty buffer' message and :qa! line.
  array_pop($lines);
  array_pop($lines);
  return $lines;
}

/**
 * Run several vim (ex) commands and return the output.
 *
 * The calling function is reponsible for validating that $vim is executable.
 *
 * @param String $vim
 *   The name of the vim executable.
 * @param Array $commands
 *   The vim commands to execute.
 * @return Array
 *   The output of $command as an array of strings or FALSE on error.
 */
function vimrc_vim_commands($vim, $commands) {
  $command = implode("\n", $commands);
  drush_print_r($commands);
  echo "command:\n$command\n--\n";
  $lines = vimrc_vim_command($vim, $command);
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
    drush_log(dt('Creating directory %autoload.',
      array('%autoload' => "$vimfiles/autoload")));
    mkdir("$vimfiles/autoload");
    $url =
      'https://raw.github.com/tpope/vim-pathogen/master/autoload/pathogen.vim';
    drush_log(dt('Downloading pathogen from %url.', array('%url' => $url)));
    $pathogen = file_get_contents($url);
    drush_log(dt('Saving it to %pathogen.',
      array('%pathogen' => "$vimfiles/autoload/pathogen")));
    file_put_contents("$vimfiles/autoload/pathogen.vim", $pathogen);
  }

  // Now, add a line to the vimrc file. If it is empty, start with the
  // standard one distributed with this script.
  $runcommands = file_get_contents($vimrc);
  // Why do I get a message that __DIR__ is undefined?
  $DIR = dirname(__FILE__);
  $bundle = realpath($DIR . '/bundle');
  $call_pathogen = "call pathogen#infect('$bundle')";
  if (strpos($runcommands, $call_pathogen) === FALSE) {
    if (empty($runcommands)) {
      $runcommands = file_get_contents($DIR . '/examples/vimrc_standard.vim');
    }
    $comment = '" Following line added by drush vimrc-install on ' . date('r');
    drush_log(dt('Rewriting vimrc file %vimrc.', array('%vimrc' => $vimrc)));
    file_put_contents($vimrc, "$comment\n$call_pathogen\n\n$runcommands");
  }

  // Update the help tags, using the pathogen variant of :helptags.
  drush_log(dt('Updating help tags.', array()));
  $lines = vimrc_vim_command($vim, 'call pathogen#helptags()');

  return;
}
