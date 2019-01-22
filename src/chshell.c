/*
 * chshell.c
 *
 * A naive Linux shell written in ISO C99.
 *
 *  ------------------------------------------------------------------------------
 * Project Information
 * ------------------------------------------------------------------------------
 *
 * The shell was developed for educational purposes.
 * Specifically, it was a continuation of the development process that was
 * required as a semester project in the course:
 *     - ΗΥ1501 - Operating Systems
 *     - Prof. N. Pitsianis, Prof. G. Chasapis
 *     - Electrical & Computer Engineering Department ( ECE )
 *       Aristotle University of Thessaloniki ( AUTh )
 *
 * ------------------------------------------------------------------------------
 *
 * ------------------------------------------------------------------------------
 * Legal Information
 * ------------------------------------------------------------------------------
 *
 * MIT License

 * Copyright (c) 2018 Athanasios D. Charisoudis
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * ------------------------------------------------------------------------------
 *
 */

/*
 * NOTES
 *
 * -----------------------------------------------
 * Signals
 * -----------------------------------------------
 * The main signals used are SIGUSR1 and SIGUSR2:
 *  - SIGUSR1 can be sent only be root process
 *  - SIGUSR2 can be received only by root process
 * -----------------------------------------------
 *
 */
#include "chshell.h"

/*
 * ----------------------
 * Shell Command methods
 * ----------------------
 *
 */
bool sh_cmd_isempty ( sh_cmd_t *cmd ) {
    return strlen( cmd->cmd ) <= 0;
}
bool sh_cmd_isvalid ( sh_cmd_t *cmd ) {

    // Vars
    pid_t pid;
    char *result, *result_f;

    // If cmd is empty is always not valid
    if ( cmd->utils->isempty( cmd ) ) return false;

    // If cmd is builtin, always passes this validity test
    if ( cmd->is_blt && NULL != cmd->bltcmd->cmd ) return true;

    // Create a pipe
    int pd[2];
    if ( pipe( pd ) < 0 ) {

        // Report Error
        fprintf( stdout, "\t@sh_cmd_isvalid(): pipe() error: %s\n", strerror( errno ) );

        // Return false to invalidate command
        return false;

    }

    // Fork
    pid = fork();
    if ( pid < 0 ) {

        // Print error in stdout
        fprintf( stdout, "\t@sh_cmd_isvalid(): fork() error: %s\n", strerror( errno ) );

        // Return false to inform for failure
        return false;

    }

    // Child process execs system() command
    if ( pid == 0 ) {

        // The write edge of the pipe should from now on be equal to stdout
        dup2( *( pd + WRITE_EDGE ), STDOUT_FILENO );

        // Close descriptors
        close( *( pd + READ_EDGE ) );
        close( *( pd + WRITE_EDGE ) );

        // Create query string
        char *bs = ( char * ) calloc( strlen( "type" ) + strlen( cmd->cmd ) + 2, sizeof( char ) );
        if ( NULL == bs ) {

            // Echo error
            fprintf( stdout, "\t@sh_cmd_isvalid(): calloc for $bs in child failed: %s\n", strerror( errno ) );

            // Return failure
            _exit( EXIT_FAILURE );

        }

        // Search for command
        sprintf( bs, "type %s\n", cmd->cmd );
        system( bs );

        // Free resources
        free( bs );

        // Exit child process
        _exit( EXIT_SUCCESS );

        // Parent process reads result and compares to result on failure
    } else {

        // Parent only reads to read
        close( *( pd + WRITE_EDGE ) );

        // Get result of child's job via pipe's READ_EDGE
        result = ( char * ) calloc( 100, sizeof( char ) );
        if ( NULL == result ) {

            // Echo error
            fprintf( stdout, "\t@sh_cmd_isvalid(): calloc for $result in parent failed: %s\n", strerror( errno ) );

            // Close read edge
            close( *( pd + READ_EDGE ) );

            // Return failure
            return false;

        }

        int i = 0;
        while ( read( *( pd + READ_EDGE ), ( result + i++ ), 1 ) > 0 );

        // The whole pipe was read
        close( *( pd + READ_EDGE ) );

        // Compare to result on failure
        result_f = ( char * ) calloc( 100, sizeof( char ) );
        if ( NULL == result ) {

            // Echo error
            fprintf( stdout, "\t@sh_cmd_isvalid(): calloc for $result in parent failed: %s\n", strerror( errno ) );

            // Close read edge
            close( *( pd + READ_EDGE ) );

            // Return failure
            return false;

        }
        sprintf( result_f, "%s: not found\n", cmd->cmd );

        // Get result
        bool cmp = strcmp( result, result_f ) == 0 ? false : true;

        // Free resources
        free( result );
        free( result_f );

        // Return the comparison of retrieved result with the failure's result
        return cmp;

    }

}
void sh_cmd_trim ( sh_cmd_t *cmd ) {

    // Vars
    char dest[strlen( cmd->cmd )], *orig;
    size_t i, left, right, len;

    // Init
    i = 0;
    left = 0;
    orig = cmd->cmd;

    // Replace multiple spaces with single space
    // While we're not at the end of the string, loop...
    while ( *cmd->cmd != '\0' ) {

        // Remove double spaces
        // Loop while the current character is a space, and the next
        // character is a space
        while ( *cmd->cmd == ' ' && *( cmd->cmd + 1 ) == ' ' ) cmd->cmd++; // skip to next character

        // Copy from the "source" string to the "destination" string,
        // while advancing to the next character in both
        *( dest + i++ ) = *cmd->cmd++;

    }
    *( dest + i ) = '\0';

    // Init vars
    len = strlen( dest );
    right = len;

    // Trim leading non alphanumerics
    i = 0;
    while ( !isalnum( *( dest + i++ ) ) && left++ );

    // Trim trailing non alphanumerics
    i = 1;
    while ( !( isalnum( *( dest + len - i++ ) ) || '.' == *( dest + len - ( i - 1 ) ) ) && right > 0 && right-- );

    // FIX: empty string of non-alphanumerics
    if ( right < left ) right = left;

    // Reallocate $cmd->cmd to prevent memory leaks
    cmd->cmd = ( char * ) realloc( orig, right - left + 1 );

    // Copy part of initial command string to $trimmed
    memmove( ( void * ) cmd->cmd, ( void * ) dest + left, ( right - left ) );
    *( cmd->cmd + right - left ) = '\0';

    // DEBUGGING:
    if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 2 )
        fprintf( stdout, "\t@trim(): command after sh_cmd_trim: --%s--\n", cmd->cmd );

}
void sh_cmd_parse ( sh_cmd_t *cmd ) {

    // Check if already parsed
    if ( cmd->is_prs ) return;

    // Check if empty
    if ( cmd->utils->isempty( cmd ) ) {

        // Set as empty command
        cmd->cmd = "";

        // Set terminating-only arg
        cmd->args = ( char ** ) calloc( 1, ARG_LEN_MAX );
        if ( NULL == cmd->args ) {

            // Echo error
            fprintf( stdout, "\t@sh_cmd_parse(): calloc for $cmd->args ( cmd is empty ) failed: %s\n",
                     strerror( errno ) );

            // Return failure
            return;

        }

        *cmd->args = NULL;  // ARGS_END
        cmd->nargs = 1;

        // Inform state & return
        cmd->is_prs = true;
        return;

    }

    // Vars
    char *arg, *raw;
    size_t nargs, i;

    // Get number of args ( cmd is the 1st token )
    nargs = sh_ntokens( cmd->cmd, CMD_DEL ) - 1;

    // Copy to $raw for processing
    raw = strdup( cmd->cmd );
    if ( NULL == raw ) {

        // Report error
        fprintf( stdout, "\t@sh_cmd_parse(): strdup for $raw failed: %s\n", strerror( errno ) );

        // Exit reporting failure
        return;

    }

    // Get command
    cmd->cmd = strsep( &raw, CMD_DEL );

    // Init cmd->args
    cmd->args = ( char ** ) calloc( nargs, ARG_LEN_MAX );
    if ( NULL == cmd->args ) {

        // Echo error
        fprintf( stdout, "\t@sh_cmd_parse(): calloc for $cmd->args failed: %s\n", strerror( errno ) );

        // Free resources used
        free( raw );

        // Return failure
        return;

    }

    // Get args
    // First arg is command's name
    *cmd->args = strdup( cmd->cmd );
    if ( NULL == *cmd->args ) {

        // Report error
        fprintf( stdout, "\t@sh_cmd_parse(): strdup for $cmd->args[0] failed: %s\n", strerror( errno ) );

        // Free resources
        free( raw );

        // Exit reporting failure
        return;

    }

    i = 1;  // i starts from 1, since the first argument is the command itself
    while ( ( arg = strsep( &raw, CMD_DEL ) ) != NULL ) {

        /*
         * FIX: empty argument
         * When for any reason an empty arg arrives, ignore it
         */
        if ( '<' != *arg && '>' != *arg && sh_strlen( arg ) == 0 ) continue;

        /*
         * FIX: string argument
         * If the arg is a string then strsep() will split it in spaces.
         * So, if a string delimiter is detected at the beginning, all args are concatenated until an arg that ends
         * with the same string delimiter is found.
         */
        if ( *arg == '"' || *arg == '\'' ) {

            // Vars
            char str_del;
            char arg_str[ROW_LEN_MAX] = "";

            // Init
            str_del = *arg;

            // Begin concatenating arguments
            do {
                strcat( arg_str, arg );
                strcat( arg_str, " " );   // add missing space delimiter

                // Check last char
                if ( str_del == *( arg + strlen( arg ) - 1 ) ) break;

                // Get new arg
            } while ( NULL != ( arg = strsep( &raw, CMD_DEL ) ) );

            // FIX: if not exited loop from break
            if ( NULL == arg && str_del != *( arg_str + strlen( arg_str ) - 1 ) ) {

                *( arg_str + strlen( arg_str ) ) = str_del;
                *( arg_str + strlen( arg_str ) + 1 ) = '\0';

            }

            // Save arg
            *( cmd->args + i++ ) = strndup( arg_str + 1, strlen( arg_str ) - 3 );

        } else {

            // Save arg ( strdup() will allocate memory for us )
            *( cmd->args + i ) = strdup( arg );
            if ( NULL == *( cmd->args + i ) ) {

                // Report error
                fprintf( stdout, "\t@sh_cmd_parse(): strdup for $cmd->args[%zu] failed: %s\n", i, strerror( errno ) );

                // Break out of loop
                break;

            }

            // Increment i
            i++;

        }

    }

    // Last arg should be null-termination for compatibility with execvp()
    *( cmd->args + i++ ) = ( char * ) 0;  // ARGS_END

    // Assign args' length
    cmd->nargs = i;

    // FIX: Delete unused memory
    // Free unused args ( from arg[cmd->nargs] up to arg[nargs -1] )
    for ( i = cmd->nargs; i < nargs; ++i ) free( *( cmd->args + i ) );

    //
    // Check if command is a built-in command
    // Also, assign the sh_bltcmd_t command to $cmd->bltcmd
    // The struct sh_bltcmd_t defines two fields: the command name and a function
    {
        // Search for built-in command

        // Vars
        size_t nbcmds;

        // Init
        nbcmds = sizeof( SH_BUILTIN_CMDS ) / sizeof( *SH_BUILTIN_CMDS );

        // DEBUGGING:
        if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 2 )
            fprintf( stdout, "\t@sh_cmd_parse(): # of built-in commands: %zu\n", nbcmds );

        // Loop through array
        for ( i = 0; i < nbcmds; ++i )
            if ( strcmp( ( SH_BUILTIN_CMDS + i )->cmd, cmd->cmd ) == 0 ) {

                // DEBUGGING:
                if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 2 )
                    fprintf( stdout, "\t@sh_cmd_parse(): IS a built-in command ( '%s' )\n", cmd->cmd );

                // Command found
                cmd->is_blt = true;
                cmd->bltcmd = SH_BUILTIN_CMDS + i;

                // Break out of loop
                break;

            }

        // If reaches here with i == nbcmds, no match found
        if ( i == nbcmds ) {

            // DEBUGGING:
            if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 2 )
                fprintf( stdout, "\t@sh_cmd_parse(): NOT a built-in command\n" );

            // No match found
            cmd->is_blt = false;
            cmd->bltcmd = NULL;

        }

    }

    // Free resources
    free( raw );
    free( arg );

    // Inform state
    cmd->is_prs = true;

}
void sh_cmd_inspect ( sh_cmd_t *cmd ) {

    // Opening text
    fprintf( stdout, "\t@sh_cmd_inspect(): command inspection start\n" );

    // Echo command and data
    fprintf( stdout, "\t\t--%s--\n\t\t\tempty: %s\n\t\t\tvalid: %s\n", cmd->cmd,
             cmd->utils->isempty( cmd ) ? "YES" : "NO", cmd->utils->isvalid( cmd ) ? "YES" : "NO" );

    // Echo Args
    fprintf( stdout, "\t\t-args ( %zu ):\n", cmd->nargs );
    for ( size_t j = 0; j < cmd->nargs; j++ )
        fprintf( stdout, "\t\t\targ[%zu]: %s\n", j, *( cmd->args + j ) );

    // Echo glues
    fprintf( stdout, "\t\t-glues:\n" );
    fprintf( stdout, "\t\t\tbef: '%s'\n", cmd->glue_b );
    fprintf( stdout, "\t\t\taft: '%s'\n", cmd->glue_a );

    // Echo props
    fprintf( stdout, "\t\t-props:\n" );
    fprintf( stdout, "\t\t\tparsed: '%s'\n", cmd->is_prs ? "YES" : "NO" );
    fprintf( stdout, "\t\t\tis_blt: '%s'\n", cmd->is_blt ? "YES" : "NO" );

    // Echo associated built-in command's name
    fprintf( stdout, "\t\t-blt:\n" );
    fprintf( stdout, "\t\t\tbltcmd: '%s'\n", cmd->is_blt ? cmd->bltcmd->cmd : "(null)" );

    // Closing text
    fprintf( stdout, "\t@sh_cmd_inspect(): command inspection stop\n" );

}

/*
 * ------------------
 * Shell Row methods
 * ------------------
 *
 */
/*
 * Trim whole row
 */
void sh_row_trim ( sh_row_t *row ) {

    // Vars
    char dest[strlen( row->raw )], *orig;
    size_t i, left, right, len;

    // Init
    i = 0;
    left = 0;
    orig = row->raw;

    // Replace multiple spaces with single space
    // While we're not at the end of the string, loop...
    while ( *row->raw != '\0' ) {

        // Remove double spaces
        // Loop while the current character is a space, and the next
        // character is a space
        while ( *row->raw == ' ' && *( row->raw + 1 ) == ' ' ) row->raw++; // skip to next character

        // Copy from the "source" string to the "destination" string,
        // while advancing to the next character in both
        *( dest + i++ ) = *row->raw++;

    }
    *( dest + i ) = '\0';

    // Init vars
    len = strlen( dest );
    right = len;

    // Trim leading non alphanumerics
    i = 0;
    while ( isspace( *( dest + i++ ) ) && left++ );

    // Trim trailing non alphanumerics
    i = 1;
    while ( isspace( *( dest + len - i++ ) ) && right > 0 && right-- );

    // FIX: empty string of non-alphanumerics
    if ( right < left ) right = left;

    // Reallocate $cmd->cmd to prevent memory leaks
    row->raw = ( char * ) realloc( orig, right - left + 1 );

    // Copy part of initial command string to $trimmed
    memmove( ( void * ) row->raw, ( void * ) dest + left, ( right - left ) );
    *( row->raw + right - left ) = '\0';

    // DEBUGGING:
    if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 2 )
        fprintf( stdout, "\t@trim(): row after sh_row_trim: --%s--\n", row->raw );

}
bool sh_row_iscomment ( sh_row_t *row ) {
    return *row->raw == '#';
}
/*
 * Parse row
 * Extract commands and the way commands are joined to each other, forming a shell_row_t object
 */
void sh_row_parse ( sh_row_t *row ) {

    // Vars
    char *raw, *raworig, *tmp, *tester;
    size_t sample_len, test_len, del_size, j, ncmds, i, i_real, untrimmed_len;

    // Init
    test_len = 3;                           // max str len of delimiter
    untrimmed_len = 0;                      // To be used when finding delimiter
    ncmds = sh_ntokens( row->raw, ROW_DEL );// # of commands included in row

    // Allocate commands' memory
    row->cmds = ( sh_cmd_t * ) malloc( ncmds * sizeof( sh_cmd_t ) );
    if ( NULL == row->cmds ) {

        // Echo error
        fprintf( stdout, "\t@parse(): malloc for $row->cmds failed: %s\n", strerror( errno ) );

        // FIX: set $row->ncmds to 0
        row->ncmds = 0;

        // Return failure
        return;

    }

    // Get part after $tmp to check for the delimiter
    tester = ( char * ) calloc( test_len, sizeof( char ) );
    if ( NULL == tester ) {

        // Echo error
        fprintf( stdout, "\t@parse(): calloc for $tester failed: %s\n", strerror( errno ) );

        // Break out of loop
        return;

    }

    // Backup raw
    raw = ( char * ) malloc( strlen( row->raw ) + 1 );
    if ( NULL == raw || NULL == strcpy( raw, row->raw ) ) {

        // Report error
        fprintf( stdout, "\t@parse(): malloc / strcpy for $raw failed: %s\n", strerror( errno ) );

        // Free resources
        free( row->raw );

        // FIX: set $row->ncmds to 0
        row->ncmds = 0;

        // Exit reporting failure
        return;

    }
    raworig = raw;

    // Get Commands
    for ( i = 0, i_real = 0; i < ncmds; ++i ) {

        // Get command
        tmp = strsep( &raw, ROW_DEL );
        if ( NULL == tmp ) {

            // Report error
            fprintf( stdout, "\t@parse(): strsep for $tmp failed: %s\n", strerror( errno ) );

            // Break out of loop
            break;

        }

        // FIX: check if tmp is empty (or has spaces)
        if ( sh_strlen( tmp ) == 0 ) continue;

        // Single command init
        ( row->cmds + i_real )->cmd = ( char * ) malloc( strlen( tmp ) + 1 );
        if ( NULL == ( row->cmds + i_real )->cmd || NULL == strcpy( ( row->cmds + i_real )->cmd, tmp ) ) {

            // Report error
            fprintf( stdout, "\t@parse(): malloc / strcpy for $row->cmds[%zu].cmd failed: %s\n", i_real, strerror( errno ) );

            // Break out of loop
            break;

        }

        // Set utils
        ( row->cmds + i_real )->utils = cmdutils;

        // Set glues
        // 1 ) Before
        ( row->cmds + i_real )->glue_b = i_real == 0 ? "" : ( row->cmds + i_real - 1 )->glue_a;
        // 2 ) After
        ( row->cmds + i_real )->glue_a = ""; // Default value
        if ( i < ncmds - 1 ) {

            // Init
            untrimmed_len += strlen( tmp );
            del_size = sizeof( DEL_ARR ) / sizeof( *DEL_ARR );

            sprintf( tester, "%.*s", ( int ) test_len, row->raw + untrimmed_len );

            // Check with all delimiters until one found
            for ( j = 0; j < del_size; ++j ) {

                // Get sample's length
                sample_len = strlen( *( DEL_ARR + j ) );

                // Test the part after string with sample
                if ( !strncmp( *( DEL_ARR + j ), tester, sample_len ) ) {

                    // Assign value
                    ( row->cmds + i_real )->glue_a = strdup( *( DEL_ARR + j ) );
                    if ( NULL == ( row->cmds + i_real )->glue_a ) {

                        // Report error
                        fprintf( stdout, "\t@parse(): strdup for $row->cmds[%zu].glue_a failed: %s\n", i_real,
                                 strerror( errno ) );

                        // Break out of loop
                        break;

                    }

                    // Update $untrimmed_len
                    untrimmed_len += strlen( *( DEL_ARR + j ) );

                    // Finish searching
                    break;

                }
            }

            // If reaches here ( not from brake ), no delimiter found, the default is will be used
            if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 ) {

                if ( j == del_size )
                    fprintf( stdout, "\t@parse(): ERROR, no delimiter found\n" );
                else
                    fprintf( stdout, "\t@parse(): delimiter found: '%s'\n", ( row->cmds + i_real )->glue_a );

            }

        }

        // Single command parsing: trim
        ( row->cmds + i_real )->utils->trim( row->cmds + i_real );

        // Increment real i
        i_real++;

    }

    // DEBUGGING:
    if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
        fprintf( stdout, "\t@parse(): %zu commands parsed\n", i_real );

    // Assign total nb
    row->ncmds = i_real;

    // Free resources
    free( raworig );
    free( tester );

}
void sh_row_inspect ( sh_row_t *row ) {

    // Opening text
    fprintf( stdout, "\t@inspect(): row inspection start\n" );

    // Inspect commands
    for ( size_t i = 0; i < row->ncmds; i++ ) {

        // Before parsing
        fprintf( stdout, "\tRAW: --%s--\n\tPARSED:\n", ( row->cmds + i )->cmd );

        // Parse
        ( row->cmds + i )->utils->parse( row->cmds + i );

        // Inspect
        if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
            ( row->cmds + i )->utils->inspect( row->cmds + i );

    }

    // Closing text
    fprintf( stdout, "\t@inspect(): row inspection stop\n" );

}
bool sh_row_exec ( sh_row_t *row ) {

    // Print formal params
    if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
        fprintf( stdout, "\t@exec(): executing row's commands ( raw: --%s-- )\n", row->raw );

    // Vars
    size_t from, ncmds, i;
    bool entered, result_ov;

    // Init
    from = 0;
    ncmds = 1;  // FIX
    entered = false;
    result_ov = true;

    /*
     * Split row execution to command-sets
     *
     * Search $row->cmds until an end major-command-set delimiter is found (';' or '&')
     * Then, execute each major command-set in sh_exec_major_command_set().
     *
     */
    for ( i = 0; i < row->ncmds; ++i, ++ncmds ) {

        // Check @row->cmds[i] 's glue_a if it matches any of the major end delimiters
        // Also, if reached last command, then enter as if last command's glue_a was ';'
        if ( ';' == *( row->cmds + i )->glue_a || i == row->ncmds - 1 ) {

            // Command-set end found: ';'
            result_ov = sh_exec_major_command_set( row, from, ncmds );

            // Update $entered flag
            if ( !entered ) entered = true;

            // Reset $from, $ncmds
            from = i + 1;
            ncmds = 0;

        }

    }

    // Return whole row's execution result
    // This will be shell's output status
    return result_ov;

}

/*
 * ----------
 * Utilities
 * ----------
 *
 */
size_t sh_ntokens ( const char *haystack, const char *del ) {

    // Vars
    char *raw, *tmp;
    size_t nargs;

    // Copy to $raw for processing
    raw = strdup( haystack );
    if ( NULL == raw ) {

        // Report error
        fprintf( stdout, "\t@sh_ntokens(): strdup for $raw failed: %s\n", strerror( errno ) );

        // Return ( ntokens is returned as zero, for safety )
        return 0;

    }

    // Get number of args using strpbrk()
    tmp = strpbrk( raw, del );
    nargs = 1;  // the first token is from the first strpbrk()
    while ( tmp != NULL ) {
        tmp = strpbrk( tmp + 1, del );
        nargs++;
    }

    // Free resources
    free( raw );
    free( tmp );

    return nargs;

}
bool sh_quit ( const char *raw ) {

    // Vars
    char *tmp, *quit;
    bool equals;
    int i;

    // Init
    tmp = ( char * ) calloc( strlen( raw ), sizeof( char ) );
    if ( NULL == tmp ) {

        // Echo error
        fprintf( stdout, "\t@sh_quit(): calloc for $quit failed: %s\n", strerror( errno ) );

        // Return failure ( true in order to quit )
        return true;

    }

    quit = "quit";

    // Copy & cvt string to lower
    for ( i = 0; *( raw + i ); i++ ) {
        *( tmp + i ) = ( char ) tolower( *( raw + i ) );
    }

    // Decide
    equals = strncmp( tmp, quit, strlen( quit ) ) == 0 ? true : false;

    // Free resources
    free( tmp );

    // Return
    return equals;

}
void sh_cls ( void ) {

    // Thanks to:
    // https://stackoverflow.com/a/17271636
    fprintf( stdout, "\033[H\033[J" );

//    // Vars
//    char *buf;
//
//    // Alloc $buf
//    buf = ( char * ) calloc( 1024, sizeof( char ) );
//    if ( NULL == buf ) {
//
//        // Report error
//        fprintf( stdout, "\t@sh_cls(): calloc for $buf failed: %s\n", strerror( errno ) );
//
//        // Return failure
//        return;
//
//    }
//
//    // Use termcap's tgetstr() to fill stdout
//    tgetent( buf, getenv( "TERM" ) );
//    if ( EOF == fputs( tgetstr( "cl", NULL ), stdout ) ) {
//
//        // Report error
//        fprintf( stdout, "\t@sh_cls(): fputs failed: %s\n", strerror( errno ) );
//
//    }
//
//    // Free resources
//    free( buf );

}
void sh_inspect_pipes ( int **pc, size_t n ) {

    // Inspect pipes
    fprintf( stdout, "\t<-------------------------------------->\n" );
    fprintf( stdout, "\t\t  idx\t|\tread\t|\twrite\n" );
    fprintf( stdout, "\t\t--------|-----------|---------\n" );
    for ( int i = 0; i < n; ++i ) {
        fprintf( stdout, "\t\t  %03d\t|\t %02d \t|\t %02d\n", i, pc[ i ][ READ_EDGE ], pc[ i ][ WRITE_EDGE ] );
    }
    fprintf( stdout, "\t<-------------------------------------->\n" );

}
/*
 * Get real strlen(), excluding non-alphanumeric characters
 * Check each char in $str is is alnum() and only if TRUE, increment length
 *
 * @param str [string]: The input string
 */
size_t sh_strlen ( const char *str ) {

    // Vars
    size_t len;

    // Count char-by-char
    len = 0;
    while ( *str ) if ( isalnum( *str++ ) || '.' == *( str - 1 ) ) len++;

    // Return real length
    return len;

}
/*
 * Search command's arguments to find '<' character
 *
 * @return [int]: argument index or 0 if no such argument found in command's args
 */
size_t sh_cmd_lt_arg_exists ( sh_cmd_t *cmd ) {

    // Vars
    size_t i;

    // Make sure command has been parsed
    if ( !cmd->is_prs ) cmd->utils->parse( cmd );

    // Search all args
    for ( i = 1; i < cmd->nargs - 1; ++i ) {

        // Compare
        if ( strncmp( "<", *( cmd->args + i ), 1 ) == 0 ) {

            // DEBUGGING:
            if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 2 )
                fprintf( stdout, "\t@sh_cmd_lt_arg_exists(): '<' found in arg[ %zu ]\n", i );

            return i;
        }

    }

    // Return 0, to indicate for failure
    return 0;

}
/*
 * Search command's arguments to find '>' character
 *
 * @return [int]: argument index or 0 if no such argument found in command's args
 */
size_t sh_cmd_gt_arg_exists ( sh_cmd_t *cmd ) {

    // Vars
    size_t i;

    // Make sure command has been parsed
    if ( !cmd->is_prs ) cmd->utils->parse( cmd );

    // Search all args
    for ( i = 1; i < cmd->nargs - 1; ++i ) {

        // Compare
        if ( strncmp( ">", *( cmd->args + i ), 1 ) == 0 ) {

            // DEBUGGING:
            if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 2 )
                fprintf( stdout, "\t@sh_cmd_gt_arg_exists(): '>' found in arg[ %zu ]\n", i );

            return i;
        }

    }

    // Return 0, to indicate for failure
    return 0;

}
/*
 * Remove $nargs from $cmd->args starting from $from
 * If command has arguments from $from + $args and on these will be brought down to $from
 *
 * @param [sh_cmd_t] cmd: command to be processed
 * @param [size_t] from: first arg to be removed
 * @param [size_t] nargs: number of arguments to be removed including arg at $from index
 * @return [bool]: FALSE if a remove fails, TRUE otherwise
 */
bool sh_cmd_purge_args( sh_cmd_t *cmd, size_t from, size_t nargs ) {

    // Check validity of $from, $nargs
    if ( from < 1 || from + nargs > cmd->nargs ) return false;

    // Vars
    size_t i, fi;

    // Begin freeing mem and replacing pointers
    for ( i = from, fi = from + nargs; i < from + nargs; ++i, ++fi ) {

        // Free memory
        free( *( cmd->args + i ) );

        // Update pointers
        *( cmd->args + i ) = *( cmd->args + fi );

    }

    // Update nargs of $cmd
    cmd->nargs -= nargs;

    // All done
    return true;

}
/*
 * Check if the filename trimmed from raw input exists and is readable
 *
 * @return [string]: Returns the trimmed filename or NULL if no file found
 */
char *sh_file_exists ( const char *raw ) {

    // DEBUGGING:
    if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
        fprintf( stdout, "\t@sh_file_exists(): searching for file: '%s'\n", raw );

    // Vars
    char *fname;

    // Alloc memory
    fname = ( char * ) malloc( strlen( raw ) + 1 );
    if ( NULL == fname ) {

        // Report error
        fprintf( stdout, "\t@sh_file_exists(): malloc for $fname failed: %s\n", strerror( errno ) );

        // Return failure
        return NULL;

    }

    // Copy $raw ( to leave $raw untouched - retain const property )
    if ( NULL == strcpy( fname, raw ) ) {

        // Report error
        fprintf( stdout, "\t@sh_file_exists(): strcpy for $fname failed: %s\n", strerror( errno ) );

        // Free resources
        free( fname );

        // Return failure
        return NULL;

    }

    // Trim non-alphanumeric characters from file name
    if ( isspace( *( fname + strlen( fname ) - 1 ) ) ) *( fname + strlen( fname ) - 1 ) = '\0';

    // decide & return
    FILE *fp = fopen( fname, "r" );
    if ( NULL != fp ) {

        // close descriptor
        fclose( fp );

        // returned the trimmed fname
        return fname;

    }

    // Free resources
    free( fname );

    // Return failure
    return NULL;

}

/*
* ------------------
* Get env methods
* ------------------
*
*/
/*
 * Get env var with named after $key
 *
 * Try searching in env, if found sh_cmd_parse value and return. If not found
 * set the default value and return the $def default value
 *
 * @param [string] key: The key to search in getenv()
 * @param [int] def: The default value returned if getenv() fails
 *                   This value is also set in env of $key
 *
 */
int sh_get_env ( const char *key, const int def ) {

    // Vars
    char *value, *test;
    int val;

    // Get env variable
    value = getenv( key );
    if ( NULL == value ) {

        // Error getting env variable
        // Return default value
        return def;

    }

    // Parse value to integer
    val = ( int ) strtol( value, &test, 10 );
    if ( value == test ) {

        // Strtol failed
        // Revert to default value
        return def;

    }

    // returned retrieved value
    return val;

}
void sh_set_env ( const char *key, int val ) {

    // Vars
    char *valstr;

    // Alloc mem for valstr
    valstr = ( char * ) calloc( ROW_LEN_MAX, sizeof( char ) );
    if ( NULL == valstr ) {

        // Report error
        fprintf( stdout, "\t@sh_set_env(): calloc for $valstr failed: %s\n", strerror( errno ) );

        // Return failure
        return;

    }

    // Convert int $val to string via sprintf
    if ( 0 > sprintf( valstr, "%d", val ) ) {

        // sprintf() failed
        // Report error
        fprintf( stdout, "\t@sh_set_env(): sprintf failed: %s\n", strerror( errno ) );

        // Free resources
        free( valstr );

        // Return failure
        return;

    }

    // Set environment variable
    if ( -1 == setenv( key, valstr, 1 ) ) {

        // setenv() failed
        // Report error
        fprintf( stdout, "\t@sh_set_env(): setenv failed: %s\n", strerror( errno ) );

    }

    // Free resources
    free( valstr );

}

/*
 * ---------
 * Printers
 * ---------
 *
 */
void sh_prt_welcome ( void ) {

    fprintf( stdout, "\n" );
    fprintf( stdout, "=================================\n" );
    fprintf( stdout, "     Welcome to CHShell v%s\n", VERSION );
    fprintf( stdout, "---------------------------------\n" );
    fprintf( stdout, "Author: Charisoudis D. Athanasios\n" );
    fprintf( stdout, "=================================\n" );
    fprintf( stdout, "\n" );

}
void sh_prt_bye ( void ) {

    fprintf( stdout, "\n" );
    fprintf( stdout, "Thanks for using this shell\n" );
    fprintf( stdout, "=================================\n" );
    fprintf( stdout, "          CHShell v%s\n", VERSION );
    fprintf( stdout, "---------------------------------\n" );
    fprintf( stdout, "Author: Charisoudis D. Athanasios\n" );
    fprintf( stdout, "=================================\n" );
    fprintf( stdout, "\n" );

}

/*
 * ----------------
 * Signal Handlers
 * ----------------
 *
 */
void sh_SIGCHLD_handler ( int sig ) {

    // Check signal
    if ( SIGCHLD != sig ) {

        // Report error
        fprintf( stdout, "\t@sh_SIGCHLD_handler(): incorrect signal caught ( caught: %d )\n", sig );

        // Return failure
        return;

    }

}
void sh_SIGUSR2_handler ( int sig ) {

    // Check signal
    if ( SIGUSR2 != sig ) {

        // Report error
        fprintf( stdout, "\t@sh_SIGUSR2_handler(): incorrect signal caught ( caught: %d )\n", sig );

        // Return failure
        return;

    }

    // Check if is main process that enters
    if ( SH_PID != getpid() ) {

        // Report error
        fprintf( stdout, "\t@sh_SIGUSR2_handler(): only main process should receive a SIGUSR2 signal\n" );

        // Return failure
        return;

    }

    // DEBUGGING:
    if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 2 ) {

        fprintf( stdout, "\t@sh_SIGUSR2_handler(): main process was sent a SIGUSR2\n" );
        fprintf( stdout, "\t@sh_SIGUSR2_handler(): will try to read from shared memory\n" );

    }

    // Vars
    pid_t cpid;
    sh_cmd_t cmd;
    sh_bltcmd_t bltcmd;
    size_t temp, add = 0, i;
    bool result;

    /*
    * -----------------------------------------------
    * Transmission Protocol
    * -----------------------------------------------
    *
    * The sent data stored in memory are the following:
    *  - [pid_t]:      pid
    *  - [size_t]:     strlen(cmd)
    *  - [char *]      cmd
    *  - [size_t]:     nargs
    *  - [size_t]:     strlen(arg{0})
    *  - [char *]      arg{0}
    *  - [size_t]:     strlen(arg{1})
    *  - [char *]      arg{1}
    *  -        ...
    *  - [size_t]:     strlen(arg{n-1})
    *  - [char *]      arg{n-1}
    *  - [size_t]:     strlen(glue_b)
    *  - [char *]      glue_b
    *  - [size_t]:     strlen(glue_a)
    *  - [char *]      glue_a
    *  - [bool]:       is_blt
    *  - [bltcmd]      bltcmd
    *
    * -----------------------------------------------
    *
    */
    // pid
    memcpy( &cpid, SH_SHARED_MEMORY_PTR + add, sizeof( pid_t ) );
    add += sizeof( pid_t );
    // strlen(cmd)
    memcpy( &temp, SH_SHARED_MEMORY_PTR + add, sizeof( size_t ) );
    add += sizeof( size_t );
    // Init cmd->cmd
    cmd.cmd = ( char * ) calloc( temp, sizeof( char ) );
    // cmd
    memcpy( cmd.cmd, SH_SHARED_MEMORY_PTR + add, temp * sizeof( char ) );
    add += temp * sizeof( char );
    // nargs
    memcpy( &temp, SH_SHARED_MEMORY_PTR + add, sizeof( size_t ) );
    add += temp * sizeof( size_t );
    // Init args
    cmd.nargs = temp;
    cmd.args = ( char ** ) calloc( temp, sizeof( *cmd.args ) );
    // args
    for ( i = 0; i < cmd.nargs - 1; ++i ) {

        // strlen( arg( i ) )
        memcpy( &temp, SH_SHARED_MEMORY_PTR + add, sizeof( size_t ) );
        add += temp * sizeof( size_t );

        // Alloc arg( i )
        *( cmd.args + i ) = ( char * ) calloc( ARG_LEN_MAX, sizeof( char ) );

        // arg( i )
        memcpy( *( cmd.args + i ), SH_SHARED_MEMORY_PTR + add, temp * sizeof( char ) );
        add += temp * sizeof( char );

    }
    // Last arg is a null argument
    *( cmd.args + cmd.nargs - 1 ) = NULL;  // ARGS_END
    // strlen(glue_b)
    memcpy( &temp, SH_SHARED_MEMORY_PTR + add, sizeof( size_t ) );
    add += sizeof( size_t );
    // Init glue_b
    cmd.glue_b = ( char * ) calloc( temp, sizeof( char ) );
    // glue_b
    memcpy( cmd.glue_b, SH_SHARED_MEMORY_PTR + add, temp * sizeof( char ) );
    add += temp * sizeof( char );
    // strlen(glue_a)
    memcpy( &temp, SH_SHARED_MEMORY_PTR + add, sizeof( size_t ) );
    add += sizeof( size_t );
    // Init glue_b
    cmd.glue_a = ( char * ) calloc( temp, sizeof( char ) );
    // glue_a
    memcpy( cmd.glue_a, SH_SHARED_MEMORY_PTR + add, temp * sizeof( char ) );
    add += temp * sizeof( char );
    // is_blt
    memcpy( &cmd.is_blt, SH_SHARED_MEMORY_PTR + add, sizeof( bool ) );
    add += sizeof( bool );
    // Get bltcmd
    memcpy( &bltcmd, SH_SHARED_MEMORY_PTR + add, sizeof( sh_bltcmd_t ) );
    // Copy
    cmd.bltcmd = &bltcmd;
    // cmd is now parsed
    cmd.is_prs = true;

    // Assign methods
    cmd.utils = cmdutils;

    // Re-construction finished
    if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 2 )
        fprintf( stdout, "\t@sh_SIGUSR2_handler(): memcpy finished. start response to $cpid = %d\n", cpid );

    // Inspect $cmd
    if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 2 ) sh_cmd_inspect( &cmd );

    // Execute
    result = cmd.bltcmd->exec( &cmd );

    // Write result to memory
    memcpy( SH_SHARED_MEMORY_PTR, &result, sizeof( bool ) );

    // Free resources
    free( cmd.cmd );
    for ( i = 0; i < cmd.nargs; ++i ) {

        // Check arg and free
        if ( NULL != cmd.args[ i ] )
            free( cmd.args[ i ] );

    }
    free( cmd.args );

    // Send a signal to child that we finished
    kill( cpid, SIGUSR1 );

}
void sh_SIGINT_handler ( int sig ) {

    //Check signal
    if ( SIGINT != sig ) {

        // Report error
        fprintf( stdout, "\t@sh_SIGINT_handler(): incorrect signal caught ( caught: %s )", strsignal( sig ) );

        // Return failure
        return;

    }

    // Vars
    pid_t pid;

    // Get entered processes' pid
    pid = getpid();

    // Check the id of entered process
    // main process shows the output screen and toggles $SH_QUIT
    // child processes exit here
    if ( SH_PID == pid ) {

        // DEBUGGING:
        if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
            fprintf( stdout, "\t@sh_SIGINT_handler(): stopping due to SIGINT receipt\n" );

        // Close stdin
        if ( EOF == fclose( stdin ) ) {

            // Report error
            fprintf( stdout, "\t@sh_SIGINT_handler(): stdin: error closing file: %s\n", strerror( errno ) );

        }

        // Check if currently executing a command
        // If a command is executing and SH_FORCE_QUIT is false then set it to true
        // else kill children as well as parent
        if ( SH_EXECUTING && !SH_FORCE_QUIT ) {

            // Raise the SH_QUIT, SH_FORCE QUIT
            SH_QUIT = true;
            SH_FORCE_QUIT = true;

            // Exit waiting for next signal
            return;

        }

        // Show bye screen
        sh_prt_bye();

        // Kill all process in group
        killpg( SH_PGID, SIGTERM );

    } else {

        // DEBUGGING:
        if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
            fprintf( stdout, "\t@sh_SIGINT_handler(): child dying due to SIGINT receipt\n" );

        // Kill child by sending it a SIGTERM signal
        kill( pid, SIGTERM );

    }

}
void sh_SIGIGN_handler ( int sig ) {

    // DEBUGGING:
    if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 2 )
        fprintf( stdout, "\t@sh_SIGIGN_handler(): signal %d ( %s ) received and ignored\n", sig, strsignal( sig ) );

}

/*
 * ------------------
 * Built-in commands
 * ------------------
 *
 * Here we define all of are shell's built-in commands.
 * All commands' names are stored in $SH_BUILTIN_CMDS_DEF global variable
 *
 */

/*
 * Change directory
 *
 * To change current directory we simply execute C's built-in chdir().
 *
 * @param path [string]: The path to cd to. If NULL, then goes back to initial working directory ( env var: $CH_WD_I )
 *
 */
bool sh_bltcmd_cd ( const sh_cmd_t *cmd ) {

    // Check command
    if ( NULL == cmd ) {

        // Report error
        fprintf( stdout, "\t@sh_bltcmd_cd(): unable to parse NULL argument\n" );

        // Return failure
        return false;

    }

    // Vars
    const char *tmp;

    // Check $path
    if ( NULL == *( cmd->args + 1 ) ) {

        // DEBUGGING:
        if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
            fprintf( stdout, "\t@sh_bltcmd_cd(): no path given, setting path to home: %s\n", getenv( "SH_WD_I\n" ) );

        // No path given
        // cd to $SH_WD_I
        tmp = SH_WD_I;

    } else {

        // Path given
        tmp = *( cmd->args + 1 );

    }

    // Change directory
    if ( chdir( tmp ) == -1 ) {

        // Report error
        fprintf( stdout, "\t@sh_bltcmd_cd(): error changing directory ( PATH = %s ): %s\n", tmp, strerror( errno ) );

        // Return failure
        return false;

    }

    // If reached here, directory changed successfully
    // Update $SH_WD via getcwd()
    getcwd( SH_WD, 1024 );
    if ( NULL == SH_WD ) {

        // Report error
        fprintf( stdout, "\t@sh_bltcmd_cd(): strdup for $SH_WD failed: %s\n", strerror( errno ) );

        // Return failure ( FALSE )
        return false;

    }

    // Return success
    return true;

}
bool sh_bltcmd_set ( const sh_cmd_t *cmd ) {

    // Check command
    if ( NULL == cmd ) {

        // Report error
        fprintf( stdout, "\t@sh_bltcmd_set(): unable to parse NULL argument\n" );

        // Return failure
        return false;

    }

    // Vars
    char *value, glue;
    size_t nargs, i;      // The number of arguments to be glued together and stored to value

    // Check args
    if ( cmd->nargs < 4 || NULL == *( cmd->args + 1 ) || NULL == *( cmd->args + 2 ) ) {

        // DEBUGGING:
        fprintf( stdout, "\t@sh_bltcmd_set(): wrong invocation: usage set VARNAME VARVALUE\n" );

        // No arg given
        return false;

    }

    // Allocate $value
    value = ( char * ) calloc( ROW_LEN_MAX, sizeof( char ) );
    if ( NULL == value ) {

        // Report error
        fprintf( stdout, "\t@sh_bltcmd_set(): calloc for $value failed: %s\n", strerror( errno ) );

        // Return failure
        return false;

    }

    // Get number of useful arguments
    nargs = cmd->nargs - 4;

    // Init value with first useful argument
    sprintf( value, "%s", *( cmd->args + 2 ) );

    // Append all other arguments, gluing them with ';'
    glue = ';';
    for ( i = 0; i < nargs; ++i ) {

        // Add glue
        strncat( value, &glue, 1 );

        // Add next argument
        strcat( value, *( cmd->args + 3 + i ) );

    }

    // Set variable with setenv()
    if ( setenv( *( cmd->args + 1 ), value, 1 ) == -1 ) {

        // Report error
        if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
            fprintf( stdout, "\t@sh_bltcmd_set(): error setting env variable: %s\n", strerror( errno ) );

        // Free resources
        free( value );

        // Return failure
        return false;

    }

    // Free resources
    free( value );

    // Return success
    return true;

}
bool sh_bltcmd_unset ( const sh_cmd_t *cmd ) {

    // Check command
    if ( NULL == cmd ) {

        // Report error
        fprintf( stdout, "\t@sh_bltcmd_unset(): unable to parse NULL argument\n" );

        // Return failure
        return false;

    }

    // Check args
    if ( cmd->nargs < 3 || NULL == *( cmd->args + 1 ) ) {

        // DEBUGGING:
        fprintf( stdout, "\t@sh_bltcmd_unset(): wrong invocation: usage set VARNAME\n" );

        // No arg given
        return false;

    }

    // Set variable with setenv()
    if ( unsetenv( *( cmd->args + 1 ) ) == -1 ) {

        // Report error
        if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
            fprintf( stdout, "\t@sh_bltcmd_unset(): error unsetting env variable: %s\n", strerror( errno ) );

        // Return failure
        return false;

    }

    // Return success
    return true;

}
bool sh_bltcmd_get ( const sh_cmd_t *cmd ) {

    // Check command
    if ( NULL == cmd ) {

        // Report error
        fprintf( stdout, "\t@sh_bltcmd_get(): unable to parse NULL argument\n" );

        // Return failure
        return false;

    }

    // Vars
    const char *envvar;

    // Check args
    if ( cmd->nargs < 3 || NULL == *( cmd->args + 1 ) ) {

        // DEBUGGING:
        if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
            fprintf( stdout, "\t@sh_bltcmd_get(): wrong invocation: usage get VARNAME\n" );

        // No arg given
        return false;

    }

    // Get env variable and return it
    envvar = getenv( *( cmd->args + 1 ) );
    if ( NULL == envvar ) {

        // Report error
        fprintf( stdout, "\t@sh_bltcmd_get(): %s: error getting env variable: %s\n", *( cmd->args + 1 ),
                 strerror( errno ) );

        // Return failure
        return false;

    }

    // Echo variable
    fprintf( stdout, "%s\n", envvar );

    // Return success
    return true;

}
bool sh_bltcmd_cls ( const sh_cmd_t *cmd ) {

    // Check command
    if ( NULL == cmd ) {

        // Report error
        fprintf( stdout, "\t@sh_bltcmd_cls(): unable to parse NULL argument\n" );

        // Return failure
        return false;

    }

    // Just execute a cls()
    sh_cls();

    // Print welcome screen
    sh_prt_welcome();

    // Return success
    return true;

}
bool sh_bltcmd_sleep ( const sh_cmd_t *cmd ) {

    // Check command
    if ( NULL == cmd ) {

        // Report error
        fprintf( stdout, "\t@sh_bltcmd_sleep(): unable to parse NULL argument\n" );

        // Return failure
        return false;

    }

    // Vars
    char *testptr;
    unsigned int rt, t;

    // Execute a sleep( cmd->arg[1] )
    // Check if arg was supplied
    if ( cmd->nargs < 3 || NULL == *( cmd->args + 1 ) ) {

        // Report error
        fprintf( stdout, "\t@sh_bltcmd_sleep(): usage sleep {SECONDS}\n" );

        // Return failure
        return false;

    }

    // Parse argument to integer with strtol()
    t = ( unsigned int ) strtol( *( cmd->args + 1 ), &testptr, 10 );
    if ( *( cmd->args + 1 ) == testptr ) {

        // strtol() failed
        // Report error
        fprintf( stdout, "\t@sh_bltcmd_sleep(): strtol failed: %s\n", strerror( errno ) );

        // Return failure
        return false;

    }

    // Execute sleep()
    if ( 0 != ( rt = sleep( t ) ) ) {

        // sleep() did not slept for all time $t
        // Report error
        fprintf( stdout, "\t@sh_bltcmd_sleep(): sleep failed ( remaining time: %d seconds): %s\n", rt,
                 strerror( errno ) );

        // Return failure
        return false;

    }

    // Return success
    return true;

}
bool sh_bltcmd_help ( const sh_cmd_t *cmd ) { // TODO

    // Check command
    if ( NULL == cmd ) {

        // Report error
        fprintf( stdout, "\t@sh_bltcmd_help(): unable to parse NULL argument\n" );

        // Return failure
        return false;

    }

    // Show a not implemented message
    if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
        fprintf( stdout, "\t@sh_bltcmd_help(): not implemented yet\n" );

    return false;

}
bool sh_bltcmd_exit ( const sh_cmd_t *cmd ) {

    // Check command
    if ( NULL == cmd ) {

        // Report error
        fprintf( stdout, "\t@sh_bltcmd_exit(): unable to parse NULL argument\n" );

        // Return failure
        return false;

    }

    // Just set the global variable $SH_QUIT to TRUE
    SH_QUIT = true;

    // Return success
    return true;

}

/*
 * --------------------
 * Execution Functions
 * --------------------
 *
 */
/*
 * Parsing & Execution of raw command line input
 *
 * It creates a sh_row_t object with commands that need to be executed out of raw data.
 * Returns TRUE if all commands executed with no errors, FALSE if at least one command failed.
 *
 * To tweak behavior, try changing global variables, $ON_ROW_ERROR_ABORT and $ON_FROW_ERROR_ABORT.
 *
 * @param raw [string]: shell raw line that needs to be parsed to sh_row_t object and executed
 */
bool sh_parse_exec_row ( const char *raw ) {

    //Vars
    bool result;
    size_t i, j;
    sh_row_t row;

    // Init $row
    row.utils = rowutils;

    // Copy raw data
    row.raw = ( char * ) malloc( strlen( raw ) + 1 );
    if ( NULL == row.raw ||  NULL == strcpy( row.raw, raw ) ) {

        // Report error
        fprintf( stdout, "\t@sh_parse_exec_row(): malloc / strcpy for $row.raw failed: %s\n", strerror( errno ) );

        // Return failure
        return false;

    }

    // Trim whole row
    row.utils->trim( &row );

    // Check if row is comment or empty
    if ( sh_strlen( row.raw ) == 0 || row.utils->iscomment( &row ) )
    {
        result = true;
        row.ncmds = 0;
    }
    else
    {
        // Parse row to commands
        row.utils->parse(&row);

        // Inspect
        if (sh_get_env(SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT) >= 2)
            row.utils->inspect(&row);

        // Execute the commands in row with the exec() method
        result = row.utils->exec(&row);

        // Free resources
        for (i = 0; i < row.ncmds; ++i) {
            free((row.cmds + i)->cmd);
            for (j = 0; j < (row.cmds + j)->nargs; ++j) {

                // Check arg and free
                if (NULL != (row.cmds + i)->args[j]) {
                    free((row.cmds + i)->args[j]);
                }

            }
        }
        if (row.cmds) free(row.cmds);
    }

    // Free resources
    free(row.raw);

    // return execution result
    return result;

}
/*
 * Execution of a major command set between ';'
 *
 * Major command-set is chunked in minor command-sets that are executed by sh_exec_minor_command_set()
 * This is done to maintain parsing priorities and a bash-like shell behavior
 *
 * @param row [sh_row_t]: The row object, which holds all commands of the parsed row
 * @param idx [size_t]: The index in $raw->cmds of the first command in command-set
 * @param ncmds [size_t]: Number of commands in command-set
 *
 */
bool sh_exec_major_command_set ( const sh_row_t *row, size_t idx, size_t ncmds ) {

    // Vars
    size_t from, mcmds, i;
    bool result, result_ov;
    pid_t cpid;

    // Init
    from = idx;
    mcmds = 1;
    result_ov = true;

    /*
     * ------------------------------
     * Background Execution of MAJOR
     * ------------------------------
     *
     * Check if command set should be run in background ( last commands ends with '&' )
     * If last command is background then the command set should run in background
     *
     */
    if ( '&' == *( row->cmds + idx + ncmds - 1 )->glue_a ) {

        // DEBUGGING:
        if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
            fprintf( stdout, "\t@sh_exec_major_command_set(): running in background\n" );

        // Inform before leaving foreground
        fprintf( stdout, "[%zu] %ld\n", ncmds, ( long ) getpid() );

        // Fork a child process
        cpid = fork();
        if ( cpid < 0 ) {

            // Print error in stdout
            fprintf( stdout, "\t@sh_exec_major_command_set(): &: fork failed: %s\n", strerror( errno ) );

            // Return false to inform for failure
            return false;

        }

        // Child should execute in background
        if ( cpid == 0 ) {

            // Execute
            result = sh_exec_minor_command_set( row, from, ncmds );

            // Terminate child informing about execution result
            _exit( result ? EXIT_SUCCESS : EXIT_FAILURE );

            // Parent should put child in background and force it to execute
        } else {

            // Throw child out of foreground
            kill( cpid, SIGTTOU );

            // Child should continue if stopped in previous child
            kill( cpid, SIGCONT );

        }

        return true;

    }

    /*
     * 1) Split row execution to command-sets
     * Search $row->cmds until an end command-set delimiter is found ('&&' or '||')
     *
     * 2) Decide how to execute command-sets
     * The execution of consecutive command-sets follows these rules:
     *  - if split by '&&', they should be executed sequentially, only if previous command-set succeeded
     *  - if split by '||', they should be executed sequentially, only if previous command-set failed
     *
     */
    for ( i = idx; i < idx + ncmds; ++i, ++mcmds ) {

        // Check @row->cmds[i] 's glue_a if it matches any of the end delimiters
        if ( !strncmp( ( row->cmds + i )->glue_a, "||", 2 ) ) {

            // Command-set end found: '||'
            result = sh_exec_minor_command_set( row, from, mcmds );

            // Check $result
            // If $result is TRUE, then no need to execute next sub-command-set
            // Else, continue
            if ( !result && result_ov ) result_ov = false;
            if ( result ) {
                result_ov = true;
                break;
            }

            // Reset $from, $mcmds
            from = i + 1;
            mcmds = 0;

        } else if ( !strncmp( ( row->cmds + i )->glue_a, "&&", 2 ) ) {

            // Command-set end found: '&&'
            result = sh_exec_minor_command_set( row, from, mcmds );

            // Check $result
            // If $result is FALSE, return from major command-set with FALSE
            // Else, continue
            if ( !result && result_ov ) result_ov = false;
            if ( !result ) break;

            // Reset $from, $mcmds
            from = i + 1;
            mcmds = 0;

        } else if ( i == idx + ncmds - 1 ) {

            // Command-set end found: '&&'
            result = sh_exec_minor_command_set( row, from, mcmds );

            // Check $result
            // If $result is FALSE, return from major command-set with FALSE
            // Else, continue
            if ( !result && result_ov ) result_ov = false;

            // Reset $from, $mcmds
            from = i + 1;
            mcmds = 0;

        }

    }

    // Return the overall result of the major command-set 's execution
    return result_ov;

}
/*
 * Execution of a command-set between '&&' or '||' or '&'
 *
 * When executing this encapsulated command-set, only pipes should be considered to separate commands
 * This ensures bash-like shell behavior
 *
 * @param row [sh_row_t]: The row object, which holds all commands of the parsed row
 * @param idx [size_t]: The index in $raw->cmds of the first command in command-set
 * @param ncmds [size_t]: Number of commands in command-set
 *
 */
bool sh_exec_minor_command_set ( const sh_row_t *row, size_t idx, size_t ncmds ) {

    // Print formal params
    if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
        fprintf( stdout, "\t@sh_exec_minor_command_set(): executing command-set ( from = %zu | ncmds = %zu )\n", idx,
                 ncmds );

    // Vars
    size_t i;
    pid_t pid;
    bool result;
    int **pipe_channel;

    /*
     * --------------------------
     * Creation of pipe channels
     * --------------------------
     *
     * The number of pipe channels should be the same as the number of commands.
     *
     * Firstly, we allocate memory for the pipe channels.
     * Then, we construct each ( using pipe() ) with checking for errors.
     *
     */

    // Allocate $pipe_channel 's first dimension
    pipe_channel = ( int ** ) calloc( ncmds, sizeof( int * ) );
    if ( NULL == pipe_channel ) {

        // Echo error
        fprintf( stdout, "\t@sh_exec_minor_command_set(): calloc for $pipe_channel failed: %s\n", strerror( errno ) );

        // Return failure
        return false;

    }

    // Init pipe channels
    for ( i = 0; i < ncmds; ++i ) {

        // Allocate pipe channel
        *( pipe_channel + i ) = ( int * ) calloc( 2, sizeof( int ) );
        if ( NULL == *( pipe_channel + i ) ) {

            // Echo error
            fprintf( stdout, "\t@sh_exec_minor_command_set(): calloc for $pipe_channel[%zu] failed: %s\n", i,
                     strerror( errno ) );

            // Dealloc used memory for pipes
            free( pipe_channel );

            // Return failure
            return false;

        }

        // Create the pipe for each channel
        // If pipe() < 0, means that descriptors could not be initialized
        if ( pipe( *( pipe_channel + i ) ) < 0 ) {

            // Print error in stdout
            fprintf( stdout, "\t@sh_exec_minor_command_set(): pipe() error: %s\n", strerror( errno ) );

            // Dealloc used memory for pipes
            free( pipe_channel );

            // Return false to inform for failure
            return false;

        }

    }


    /*
     * ----------------------------
     * Execution of MINOR commands
     * ----------------------------
     *
     * To execute the commands in the minor command-set, we use two main processes.
     *  - parent process:   executes the minor commands assigning the execution to a forked process per command
     *  - child process :   listens to the READ_EDGE of the last pipe to read the final output,
     *                      then it prints to stdout and exits
     *
     * Failure:
     * In the case that a command fails ( return status non-zero ) the execution stops and parent returns FALSE.
     *
     */

    // Fork process
    pid = fork();
    if ( pid < 0 ) {

        // Print error in stdout
        fprintf( stdout, "\t@sh_exec_minor_command_set(): fork failed: %s\n", strerror( errno ) );

        // Dealloc used memory for pipes
        free( pipe_channel );

        // Return false to inform for failure
        return false;

    }

    /* Child Process */
    if ( pid == 0 ) {

        // Execute sh_exec_response
        sh_exec_response( ncmds, pipe_channel );

        // Child collected response
        _exit( EXIT_SUCCESS );

        /* Parent Process */
    } else {

        // Execute MINOR command-set and get result
        result = sh_exec_wrapper( row->cmds + idx, ncmds, pipe_channel );

        // Inspect execution result
        if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
            fprintf( stdout, "\t@sh_exec_minor_command_set(): sh_exec_wrapper() exited with status: %d\n",
                     result ? EXIT_SUCCESS : EXIT_FAILURE );

        // Wait for response collector ( do not care about return status )
        waitpid( pid, NULL, 0 );

        // UPDATE: If execution fails, try searching if command was a batch file's name
        if ( !result && sh_get_env( SH_ON_CMD_FAIL_SEARCH_BF_KEY, SH_ON_CMD_FAIL_SEARCH_BF_DEFAULT ) ) {

            // Report for redirection
            if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
                fprintf( stdout, "\t@sh_exec_wrapper(): command failed - searching for batch file '%s'\n",
                         ( row->cmds + idx )->cmd );

            // Check as filename
            char *fname;

            // Search
            fname = sh_file_exists( ( row->cmds + idx )->cmd );

            // Check returned string
            // If is not NULL, then a command is an existent file
            // Execute file in batch mode and return this result
            if ( NULL != fname ) {  // Found as filename

                // Execute
                result = mode_b( fname );

                // Free resources
                free( fname );

            }

        }

    }

    // Dealloc used memory for pipes
    for ( i = 0; i < ncmds; ++i ) free( *( pipe_channel + i ) );
    free( pipe_channel );

    // Inform MAJOR about execution result
    return result;

}
/*
 * Execution of a minor command-set
 * In MINOR command-set only pipes are allowed between commands
 */
bool sh_exec_wrapper ( sh_cmd_t *cmds, size_t ncmds, int **pd ) {

    // DEBUGGING:
    if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 2 ) {
        fprintf( stdout, "\t@sh_exec_wrapper(): pipe_channel inspection start\n" );
        sh_inspect_pipes( pd, ncmds );
        fprintf( stdout, "\t@sh_exec_wrapper(): pipe_channel inspection stop\n" );
    }

    // Vars
    bool result;
    pid_t *pid;
    size_t i;

    // Init
    // We will fork so many processes as are the commands of current MINOR
    pid = ( pid_t * ) calloc( ncmds, sizeof( pid_t ) );
    if ( NULL == pid ) {

        // Echo error
        fprintf( stdout, "\t@sh_exec_wrapper(): calloc for $pid failed: %s\n", strerror( errno ) );

        // Return failure
        return false;

    }

    result = true;      // overall result ( if any child fails, this becomes FALSE )

    // Execute commands, forking each to a child process
    for ( i = 0; i < ncmds; ++i ) {

        // File nos
        int fileno_stdin, fileno_stdout;

        // Create a process for first command
        *( pid + i ) = fork();
        if ( *( pid + i ) < 0 ) {

            // Print error in stdout
            fprintf( stdout, "\t@sh_exec_wrapper(): fork() error ( i = %zu ): %s\n", i, strerror( errno ) );

            // Break out of loop
            break;

        }

        /*
         * New command in a new process (the child process)
         *
         *  input: pd[i-1][1] (instead of STDOUT_FILENO)
         *  output: pd[i][0] (instead of STDOUT_FILENO)
         */
        if ( *( pid + i ) == 0 ) {

            /*
             * -----------
             * Setup pipe
             * -----------
             *
             * The pipe channel has been created and accessed @ $pipe_channel[ $pos ][ $READ_EDGE / $WRITE_EDGE ].
             * Before executing the command, we should setup the READ / WRITE edge of its pipe channel.
             *
             */
            if ( NULL != pd ) {

                if ( NULL != pd ) {

                    // Check if command has specified input or output file(s)
                    // For input file the arg '<' should exist in $cmd->args and a valid filename should have been given
                    // For output file the arg '>' should exist in $cmd->args and a valid filename should have been given
                    size_t lt_index, gt_index;

                    // Input check
                    lt_index = sh_cmd_lt_arg_exists( cmds + i );
                    if ( lt_index > 0 && ( cmds + i )->nargs > lt_index + 1 ) {

                        // Get input file's name
                        char *fname_in;
                        fname_in = strdup( *( ( cmds + i )->args + lt_index + 1 ) );

                        // DEBUGGING:
                        if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 2 )
                            fprintf( stdout, "\t@sh_exec_wrapper(): '%s': input file set\n", fname_in );

                        // Check if file exists
                        FILE *fpin = fopen( fname_in, "r" );
                        if ( NULL != fpin ) {

                            // Assign file no of open file stream
                            fileno_stdin = fileno( fpin );

                            // Remove arguments relevant to this operation
                            sh_cmd_purge_args( cmds + i, lt_index, 2 );

                            // DEBUGGING:
                            if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 2 )
                                ( cmds + i )->utils->inspect( cmds + i );

                        } else {

                            // Fallback to default pipe's file no
                            fileno_stdin = 0 < i ? pd[ i - 1 ][ READ_EDGE ] : STDIN_FILENO;

                        }

                    } else fileno_stdin = 0 < i ? pd[ i - 1 ][ READ_EDGE ] : STDIN_FILENO;

                    // Output check
                    gt_index = sh_cmd_gt_arg_exists( cmds + i );
                    if ( gt_index > 0 && ( cmds + i )->nargs > gt_index + 1 ) {

                        // Get input file's name
                        char *fname_out;
                        fname_out = strdup( *( ( cmds + i )->args + gt_index + 1 ) );

                        // DEBUGGING:
                        if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 2 )
                            fprintf( stdout, "\t@sh_exec_wrapper(): '%s': output file set\n", fname_out );

                        // Check if file exists
                        FILE *fpout = fopen( fname_out, "a+" );
                        if ( NULL == fpout ) {

                            // Fallback to default pipe's file no
                            fileno_stdout = pd[ i ][ WRITE_EDGE ];

                        } else {

                            // Assign open file no to fpout
                            fileno_stdout = fileno( fpout );

                            // Remove arguments relevant to this operation
                            if ( !sh_cmd_purge_args(cmds + i, gt_index, 2 ) ) {

                                // DEBUGGING:
                                if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
                                    fprintf( stdout, "\t@sh_exec_wrapper(): sh_cmd_purge_args() failed\n" );

                                // Fallback to default pipe's file no
                                fileno_stdout = pd[ i ][ WRITE_EDGE ];

                            }

                            // DEBUGGING:
                            if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 2 )
                                ( cmds + i )->utils->inspect( cmds + i );

                        }

                    } else fileno_stdout = pd[ i ][ WRITE_EDGE ];

                    // Duplicate descriptors
                    dup2( fileno_stdin, STDIN_FILENO );
                    dup2( fileno_stdout, STDOUT_FILENO );

                    // Child: Close all descriptors
                    for ( size_t ci = 0; ci < ncmds; ++ci ) {
                        close( pd[ ci ][ READ_EDGE ] );
                        close( pd[ ci ][ WRITE_EDGE ] );
                    }

                    // Check for file descriptors
                    if ( ( 0 < i && fileno_stdin != pd[ i - 1 ][ READ_EDGE ] ) || fileno_stdin != STDIN_FILENO ) close( fileno_stdin );
                    if ( fileno_stdout != pd[ i ][ WRITE_EDGE ] ) close( fileno_stdout );

                }

            }

            // Execute command and get execution result
            result = sh_exec( cmds + i );

            // Check if command is valid
            if ( !result ) {

                // If command is returned <> 0, inform to stdout
                fprintf( stdout, "\t@sh_exec_wrapper(): %s: command failed: %s\n",
                         ( cmds + i )->cmd, strerror( errno ) );

                // Close std file streams ( as would a successful execution )
                fclose( stdin );
                fclose( stdout );

                // If reached here..
                _exit( EXIT_FAILURE );

            }

            // Command executed successfully
            _exit( EXIT_SUCCESS );

        }

    }

    // Parent: Close all descriptors
    if ( NULL != pd ) {

        for ( i = 0; i < ncmds; ++i ) {

            close( pd[ i ][ READ_EDGE ] );
            close( pd[ i ][ WRITE_EDGE ] );

        }

    }

    // Parent: Wait for all known pids to die
    for ( i = 0; i < ncmds; ++i ) {

        // Get return status of dead child
        int status;
        waitpid( *( pid + i ), &status, 0 );

        // Check $status
        if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
            fprintf( stdout, "\t@sh_exec_wrapper(): -- child [pid = %d] finished with status: %d\n", *( pid + i ),
                     status );

        // Return
        if ( EXIT_SUCCESS != status ) {

            // DEBUGGING:
            if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
                fprintf( stdout, "\t@sh_exec_wrapper(): -- status states failure, aborting\n" );

            // Assign overall result to FALSE
            result = false;

            // Exit wait loop
            break;

        }

    }

    return result;

}
void sh_exec_response ( size_t ncmds, int **pd ) {

    int i;
    char buf[BUF_LEN_MAX];

    // Close all descriptors
    close( pd[ ncmds - 1 ][ WRITE_EDGE ] );
    for ( i = 0; i < ncmds - 1; ++i ) {
        close( pd[ i ][ READ_EDGE ] );
        close( pd[ i ][ WRITE_EDGE ] );
    }

    i = 0;
    while ( read( pd[ ncmds - 1 ][ READ_EDGE ], buf + i++, 1 ) > 0 );

    // Terminate string
    if ( i > 2 ) {

        *( buf + ( i - 2 ) ) = '\n';    // if not last character was new line
        *( buf + ( i - 1 ) ) = '\0';    // for normal termination

    } else {

        // No output
        *buf = '\0';

    }

    // Close pipe
    close( pd[ ncmds - 1 ][ READ_EDGE ] );

    // Print what's read
    fwrite( buf, 1, strlen( buf ), stdout );

    // Close stdout
    fclose( stdout );

}
/*
 * Executes a single command
 *
 * It forks a children which transfers execution via execvp().
 * The parent waits for the return status of its child, and if 0 returns true else returns false.
 * Does not takes care of pipes ( it is assumed that the wrapper set the pipe channel etc. )
 */
bool sh_exec ( sh_cmd_t *cmd ) {

    // DEBUGGING:
    if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
        fprintf( stdout, "\t@sh_exec(): executing --%s--\n", NULL != cmd->cmd ? cmd->cmd : "(null)\n" );

    // First sh_cmd_parse command ( if not parsed before )
    cmd->utils->parse( cmd );

    // Inspect command
    if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 2 ) cmd->utils->inspect( cmd );

    // Check if command is valid
    if ( !cmd->utils->isvalid( cmd ) ) return false;

    // Check if is built-in command
    if ( cmd->is_blt ) return sh_exec_builtin( cmd );

    // Vars
    pid_t pid = fork();

    // Check fork
    if ( pid < 0 ) {

        // Print error in stdout
        fprintf( stdout, "\t@sh_exec(): fork failed: %s\n", strerror( errno ) );

        // Return false to inform for failure
        return false;

    }

    // Child process execs command via execvp()
    if ( pid == 0 ) {

        // Execute shell command with args
        execvp( cmd->cmd, cmd->args );

        // If reaches here, means an error occured
        fprintf( stdout, "\t@sh_exec(): execvp returned: %s\n", strerror( errno ) );

        // Return failure
        _exit( EXIT_FAILURE );

        // Parent process waits for child to finish while reading output from pipe
    } else {

        // Get child status (upon return)
        int status = 0;

        // Wait for child with pid = cpid
        // Both WNOHANG, WUNTRACED are disabled, since we want blocking operation and job run in foreground
        waitpid( pid, &status, 0 );

        // After child's execution
        if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
            fprintf( stdout, "\t@sh_exec(): -- child [pid = %d] finished with status: %d\n", pid, status );

        // Return execution result
        return EXIT_SUCCESS == status;

    }

}
/*
 * Built-in command execution
 *
 * When a built-in command needs to be executed, it has to be executed on the main process.
 * But here we reach via sh_exec() and one of its children. Therefore, we have to use a shared memory strategy
 * in order to move the whole command from current process to the main process. When transfer complete, we send
 * a signal to the main process to process command.
 */
bool sh_exec_builtin ( sh_cmd_t *cmd ) {

    // DEBUGGING:
    if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
        fprintf( stdout, "\t@sh_exec_builtin(%d): executing builtin command: %s\n", getpid(), cmd->cmd );

    //Vars
    pid_t pid;                  // the entered process id
    size_t temp, add, i;
    struct sigaction sigact;    // the sigaction of current process
    sigset_t sig_block_set;     // the set of enabled signals ( negative logic )
    bool result;                // the execution result from main process

    // If built-in command can be run by any process ( e.g. reading an env variable )
    // then we should run it here
    if ( !cmd->bltcmd->run_in_main_process ) {

        // Execute built-in command's associated function
        // The struct sh_bltcmd_t defines two fields: the command name and a function
        result = cmd->bltcmd->exec( cmd );

    } else {

        // else execute command in main process by saving command in shared memory
        // and exchange hand-shaking signals with main process

        // Init
        pid = getpid();
        add = 0;

        /*
         * -----------------------------------------------------
         * Transmission Protocol
         * -----------------------------------------------------
         *
         * The sent data stored in memory are the following:
         *  - [pid_t]:      pid
         *  - [size_t]:     strlen(cmd)
         *  - [char *]      cmd
         *  - [size_t]:     nargs
         *  - [size_t]:     strlen(arg{0})
         *  - [char *]      arg{0}
         *  - [size_t]:     strlen(arg{1})
         *  - [char *]      arg{1}
         *  -        ...
         *  - [size_t]:     strlen(arg{n-1})
         *  - [char *]      arg{n-1}
         *  - [size_t]:     strlen(glue_b)
         *  - [char *]      glue_b
         *  - [size_t]:     strlen(glue_a)
         *  - [char *]      glue_a
         *  - [bool]:       is_blt
         *  - [bltcmd]      bltcmd
         *
         * The response data stored in memory are the following:
         *  - [bool]:       result
         *  - [size_t];     strlen(msg)
         *  - [char *]:     msg
         *
         * -----------------------------------------------------
         *
         */
        // pid
        memcpy( SH_SHARED_MEMORY_PTR + add, &pid, sizeof( pid_t ) );
        add += sizeof( pid_t );
        // strlen(cmd)
        temp = strlen( cmd->cmd ) + 1;
        memcpy( SH_SHARED_MEMORY_PTR + add, &temp, sizeof( size_t ) );
        add += sizeof( size_t );
        // cmd
        memcpy( SH_SHARED_MEMORY_PTR + add, cmd->cmd, temp * sizeof( char ) );
        add += temp * sizeof( char );
        // nargs
        temp = cmd->nargs;
        memcpy( SH_SHARED_MEMORY_PTR + add, &temp, sizeof( size_t ) );
        add += temp * sizeof( size_t );

        // args
        for ( i = 0; i < cmd->nargs - 1; ++i ) {

            // strlen( arg( i ) )
            temp = strlen( *( cmd->args + i ) ) + 1;
            memcpy( SH_SHARED_MEMORY_PTR + add, &temp, sizeof( size_t ) );
            add += temp * sizeof( size_t );

            // arg( i )
            memcpy( SH_SHARED_MEMORY_PTR + add, *( cmd->args + i ), temp * sizeof( char ) );
            add += temp * sizeof( char );

        }

        // strlen(glue_b)
        temp = strlen( cmd->glue_b ) + 1;
        memcpy( SH_SHARED_MEMORY_PTR + add, &temp, sizeof( size_t ) );
        add += sizeof( size_t );
        // glue_b
        memcpy( SH_SHARED_MEMORY_PTR + add, cmd->glue_b, temp * sizeof( char ) );
        add += temp * sizeof( char );
        // strlen(glue_a)
        temp = strlen( cmd->glue_a ) + 1;
        memcpy( SH_SHARED_MEMORY_PTR + add, &temp, sizeof( size_t ) );
        add += sizeof( size_t );
        // glue_a
        memcpy( SH_SHARED_MEMORY_PTR + add, cmd->glue_a, temp * sizeof( char ) );
        add += temp * sizeof( char );
        // is_blt
        memcpy( SH_SHARED_MEMORY_PTR + add, &cmd->is_blt, sizeof( bool ) );
        add += sizeof( bool );
        // bltcmd
        memcpy( SH_SHARED_MEMORY_PTR + add, cmd->bltcmd, sizeof( sh_bltcmd_t ) );

        // Send a SIGUSR2 signal to main process to inform about executing built-in command
        // Wait here until main process responds with SIGUSR
        signal( SIGUSR2, SIG_IGN );
        kill( SH_PID, SIGUSR2 );

        /*
         * ------------
         * Signal Setup
         * ------------
         *
         */
        sigfillset( &sig_block_set );
        sigdelset( &sig_block_set, SIGUSR1 );

        sigemptyset( &sigact.sa_mask );     // clears all signals
        sigact.sa_flags = 0;                // no special flag needed
        sigact.sa_handler = sh_SIGIGN_handler;        // set the handler ( dummy )
        sigaction( SIGUSR1, &sigact, NULL );

        // Wait for response signal
        sigsuspend( &sig_block_set );

        // Inform that you' ve received response
        if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
            fprintf( stdout, "\t@sh_exec_builtin(): main process finished executing command\n" );

        // Get result
        memcpy( &result, SH_SHARED_MEMORY_PTR, sizeof( bool ) );

    }

    // Return execution result
    return result;

}

/*
 * ----------------
 * Operation Modes
 * ----------------
 *
 * 1 ) Interactive Mode
 *    The user inputs single-row commands, the program parses them and executes them in order.
 *
 * 2 ) Batch Mode
 *    The user inputs a filename that contains a batch of single-row commands.
 *    The program parses and executes each row as in interactive mode
 *
 * 3 ) Mixed Mode
 *    The user inputs single-row commands that may contain just the batch file's name.
 *    In that case the program switches to batchmode and after execution of file's commands, returns to interactive mode.
 *
 */
/*
 *  mode_i() - Interactive Mode
 *
 *  In interactive mode, user enters an one-line command.
 *  The program reads the raw input and parses it to commands to be executed.
 *
 *  @param [bool] mode_m: enable / disable mixed mode
 *  @return [bool]: the overall execution result of all commands given during interactive mode
 *
 */
bool mode_i ( bool mode_m ) {

    // DEBUGGING:
    if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
        fprintf( stdout, ":| Switched to interactive mode |:\n" );

    // Vars
    char *raw, *prompt, *fname;
    bool result_partial, result_overall;

    // Init
    raw = ( char * ) calloc( ROW_LEN_MAX, sizeof( char ) );
    if ( NULL == raw ) {

        // Echo error
        fprintf( stdout, "\t@mode_i(): calloc for $raw failed: %s\n", strerror( errno ) );

        // Return failure
        return false;

    }

    prompt = ( char * ) calloc( strlen( PROMPT_MESSAGE ) + 1, sizeof( char ) );
    if ( NULL == prompt ) {

        // Echo error
        fprintf( stdout, "\t@mode_i(): calloc for $prompt failed: %s\n", strerror( errno ) );

        // Free resources
        free( raw );

        // Return failure
        return false;

    }

    result_overall = true;

    // Create prompt
    strcat( prompt, PROMPT_MESSAGE );   // we create prompt message in this way so as for it to be flexible

    // Main shell loop
    do {

        // Print prompt
        fprintf( stdout, "%s> ~%s$ ", prompt,
                 sh_get_env( SH_SHOW_WD_KEY, SH_SHOW_WD_DEFAULT ) || strcmp( SH_WD, SH_WD_I ) != 0 ? SH_WD : "" );

        // Clear the raw buffer
        memset( raw, '\0', ROW_LEN_MAX );

        // Get input
        fgets( raw, ROW_LEN_MAX, stdin );

        // Remove <lf>
        *( raw + strlen( raw ) - 1 ) = '\0';

        // Check for exit
        if ( sh_quit( raw ) ) break;

        // Raise executing flag
        SH_EXECUTING = true;

        // Drop SH_FORCE_QUIT flag
        // needs be off before every command's execution
        SH_FORCE_QUIT = false;

        // Parse & execute single-row commands
        // Check if in mixed mode and file is batch file
        if ( mode_m ) {

            // Check if filename exists
            fname = mode_m ? sh_file_exists( raw ) : NULL;

            // Check fname
            if ( NULL != fname ) {

                // If TRUE, ask if should execute file name
                fprintf( stdout, "\t@mode_i(): %s: execute batch file instead of command-set? [y|N]: ", fname );

                // Get answer
                char tempptr[ROW_LEN_MAX];
                fgets( tempptr, ROW_LEN_MAX, stdin );

                // Check answer
                if ( 'y' == *tempptr || 'Y' == *tempptr ) {

                    // Execute
                    result_partial = mode_b( fname );

                    // Free resources
                    free( fname );

                } else result_partial = sh_parse_exec_row( raw );

            } else result_partial = sh_parse_exec_row( raw );

        } else result_partial = sh_parse_exec_row( raw );

        // Drop executing flag
        SH_EXECUTING = false;

        if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
            fprintf( stdout, "\t@mode_i(): command execution finished\n" );

        // Check result
        if ( !result_partial ) {

            // Print error
            if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
                fprintf( stdout, "\t@mode_i(): error executing command-set: '%s'\n", raw );

            // Assign to result_overall
            if ( result_overall ) result_overall = false;

            // Check abort
            if ( sh_get_env( SH_ON_ROW_ERR_ABRT_KEY, SH_ON_ROW_ERR_ABRT_DEFAULT ) ) break;

        }

    } while ( !SH_QUIT );

    // Free resources
    free( raw );
    free( prompt );

    // Return
    return result_overall;
}

/*
 *  mode_b() - Batch Mode
 *
 *  The batch file is nothing more than a set of sh_row_t rows.
 *  Thus, we treat batch files as arrays of shell rows.
 */
bool mode_b ( const char *bfname ) {

    // DEBUGGING:
    if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
        fprintf( stdout, ":| Switched to batch mode |:\n" );

    // Vars
    char *bfline;
    bool result_partial, result_overall;
    FILE *bfp;

    // Init
    bfline = ( char * ) calloc( ROW_LEN_MAX, sizeof( char ) );
    if ( NULL == bfline ) {

        // Echo error
        fprintf( stdout, "\t@mode_b(): calloc for $bfline failed: %s\n", strerror( errno ) );

        // Return failure
        return false;

    }

    // Ask if user wants to open file as batch file
    fprintf( stdout, "%s: would you like to open as a batch file? (y|N): ", bfname );

    // Get answer
    fgets( bfline, ROW_LEN_MAX, stdin );

    // Check answer
    if ( 'y' == *bfline || 'Y' == *bfline ) {

        // Open file
        bfp = fopen( bfname, "r" );
        if ( bfp == NULL ) {

            // Report error
            fprintf( stdout, "\t@mode_b(): %s: error opening file: %s\n", bfname, strerror( errno ) );

            // Try closing stream
            fclose( bfp );

            // Return failure
            return false;

        }

        // Go to file start
        //rewind( bfp );

        // Read lines
        result_overall = true;
        while ( fgets( bfline, ROW_LEN_MAX, bfp ) ) {

            // Remove <lf> if last char
            if ( '\n' == *( bfline + strlen( bfline ) - 1 ) )
                *( bfline + strlen( bfline ) - 1 ) = '\0';

            // Check for exit
            if ( sh_quit( bfline ) ) break;

            // Parse & execute single-row commands
            result_partial = sh_parse_exec_row( bfline );

            // Check result
            if ( !result_partial ) {

                // Print error
                fprintf( stdout, "\t@mode_b(): error executing command-set: '%s'\n", bfline );

                // Assign to result_overall
                if ( result_overall ) result_overall = false;

                // Check abort
                if ( sh_get_env( SH_ON_FROW_ERR_ABRT_KEY, SH_ON_FROW_ERR_ABRT_DEFAULT ) ) break;

            }

            if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
                fprintf( stdout, "\n<-------------------------------->\n\n" );

        }

        // Close file pointer
        if ( EOF == fclose( bfp ) ) {

            // Report error
            fprintf( stdout, "\t@mode_b(): %s: error closing file: %s\n", bfname, strerror( errno ) );

            // Make $result_overall false
            result_overall = false;

        }

    } else {

        // DEBUGGING:
        if ( sh_get_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT ) >= 1 )
            fprintf( stdout, "\t@mode_b(): %s: user rejected opening file\n", bfname );

        // Assign result
        result_overall = false;

    }

    // Free resources
    free( bfline );

    // Return result of whole file execution
    return result_overall;

}

/*
 * -------
 * main()
 * -------
 *
 */
int main ( int argc, char **argv, char **envp ) {

    // Vars
    char *fname;      // The pointer to check if valid batch file name given

    // Set fname to the possible batch file's name, given in argv
    fname = argc == 1 || NULL == *( argv + 1 ) ? NULL : sh_file_exists( *( argv + 1 ) );

    // Set main() process id
    SH_PID = getpid();

    // Choose SH_MODE
    if ( argc == 1 || NULL == fname ) {

        /*
         * ==================
         *  INTERACTIVE MODE
         * ==================
         *
         */

        /*
         * Process setup
         *
         * Grant our own process group
         * And, since this is terminal, we want to bring our process to foreground
         *
         */

        // Check if STDIN is the terminal
        if ( isatty( STDIN_FILENO ) ) {

            // Wait for current process group to get in foreground
            // We check if tcgetpgrp( stdin ) returns the same id as getpgrp()
            // When TRUE, we know that our process group has come in to foreground
            while ( tcgetpgrp( STDIN_FILENO ) != ( SH_PGID = getpgrp() ) )
                kill( SH_PID, SIGTTIN );    // Send a signal when this process group gets to foreground

            // Create a new process group for the shell
            // To do that, we call setpgid( pid, pid ) with the pid of main()'s process
            // After that, this process should be the new process group's leader
            setpgid( SH_PID, SH_PID );

            // Get process group id
            SH_PGID = getpgrp();

            // Check if a new process group was created
            // To do that, we can check if $SH_PGID == $SH_PID
            if ( SH_PID != SH_PGID ) {

                // Report error
                fprintf( stdout, "\t@main(): error attaching process as the process group leader\n" );

                // Exit reporting failure
                exit( EXIT_FAILURE );

            }

            // Grab control of the terminal
            tcsetpgrp( STDIN_FILENO, SH_PGID );

            // Get default stdin attributes
            // These are some common properties used by our shell
            tcgetattr( STDIN_FILENO, &SH_TMODES );

            // Set orphans' subreaper for background process tracking
            prctl( PR_SET_CHILD_SUBREAPER, SH_PID );

            // Set the mode to be INTERACTIVE ( mixed )
            SH_MODE = 'm';

        } else {

            // Failed creating process group in order to start INTERACTIVE mode
            fprintf( stdout, "\t@main(): error creating process group for chshell: %s\n", strerror( errno ) );

            // Exit reporting failure
            exit( EXIT_FAILURE );

        }

    } else {

        /*
         * ============
         *  BATCH MODE
         * ============
         *
         */

        // If reaches here, means that a valid file is found
        // We should try starting in BATCH mode
        SH_MODE = 'b';

    }

    /*
     * Signals setup
     *
     * main process listens to SIGUSR2, SIGINT, SIGCHLD
     *
     */
    // The SIGUSR2 signal informs about executing a builtin command
    if ( SIG_ERR == signal( SIGUSR2, sh_SIGUSR2_handler ) ) {

        // Report error
        fprintf( stdout, "\t@main(): attaching to SIGUSR2 not honored: %s\n", strerror( errno ) );

        // Free resources
        free( SH_WD );
        free( SH_WD_I );

        // Exit reporting failure
        exit( EXIT_FAILURE );

    }

    // When Ctrl + C is pressed: show bye screen and exit
    if ( SIG_ERR == signal( SIGINT, sh_SIGINT_handler ) ) {

        // Report error
        fprintf( stdout, "\t@main(): attaching to SIGINT not honored: %s\n", strerror( errno ) );

        // Free resources
        free( SH_WD );
        free( SH_WD_I );

        // Exit reporting failure
        exit( EXIT_FAILURE );

    }

    // When child process exits, check for leftovers
    if ( SIG_ERR == signal( SIGCHLD, sh_SIGCHLD_handler ) ) {

        // Report error
        fprintf( stdout, "\t@main(): attaching to SIGCHLD not honored: %s\n", strerror( errno ) );

        // Free resources
        free( SH_WD );
        free( SH_WD_I );

        // Exit reporting failure
        exit( EXIT_FAILURE );

    }

    // Get initial current working directory
    SH_WD_I = ( char * ) calloc( DIR_LEN_MAX, sizeof( char ) );
    if ( NULL == SH_WD_I ) {

        // Report error
        fprintf( stdout, "\t@main(): calloc for $SH_WD_I failed: %s\n", strerror( errno ) );

        // Exit reporting failure
        exit( EXIT_FAILURE );

    }
    // Set with getcwd()
    if ( NULL == getcwd( SH_WD_I, DIR_LEN_MAX ) ) {

        // Report error
        fprintf( stdout, "\t@main(): getcwd failed: %s\n", strerror( errno ) );

        // Free resources
        free( SH_WD_I );

        // Exit reporting failure
        exit( EXIT_FAILURE );

    }

    // Initiate $SH_WD with the initial working directory
    SH_WD = strdup( SH_WD_I );
    if ( NULL == SH_WD ) {

        // Report error
        fprintf( stdout, "\t@main(): strdup for $SH_WD failed: %s\n", strerror( errno ) );

        // Free resources
        free( SH_WD_I );

        // Exit reporting failure
        exit( EXIT_FAILURE );

    }

    /*
     * Setup our own environment variables
     *
     * Example usage:  setenv( "key", "val", 1 );
     */
    setenv( "SH_WD_I", SH_WD_I, 1 );    // initial working directory
    sh_set_env( "SH_PID", SH_PID );     // main() process' id
    sh_set_env( SH_DBG_MODE_KEY, SH_DBG_MODE_DEFAULT );
    sh_set_env( SH_SHOW_WD_KEY, SH_SHOW_WD_DEFAULT );
    sh_set_env( SH_ON_ROW_ERR_ABRT_KEY, SH_ON_ROW_ERR_ABRT_DEFAULT );
    sh_set_env( SH_ON_FROW_ERR_ABRT_KEY, SH_ON_FROW_ERR_ABRT_DEFAULT );
    sh_set_env( SH_ON_CMD_FAIL_SEARCH_BF_KEY, SH_ON_CMD_FAIL_SEARCH_BF_DEFAULT );

    /*
     * Setup built-in command execution
     *
     * In particular we need to allocate shared memory and save the pointer.
     * The space we allocate is the space for one sh_cmd_t object + the size of pid_t to save the pid at the beginning.
     * UPDATE: since the size of sh_cmd_t object is unknown we allocate SHM_LEM_MAX memory and we use a transmission
     * protocol to transfer object to shared memory's space.
     */
    SH_SHARED_MEMORY_PTR = mmap( 0, SHM_LEN_MAX, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0 );
    if ( NULL == SH_SHARED_MEMORY_PTR ) {

        // Report error
        fprintf( stdout, "\t@main(): mmap failed allocating %d bytes of shared memory: %s\n", SHM_LEN_MAX,
                 strerror( errno ) );

        // Free resources
        if ( 'm' == SH_MODE ) {

            free( SH_WD );
            free( SH_WD_I );

        }

    }

    // Init util pointers
    cmdutils = ( sh_cmdops_t * ) calloc( 1, sizeof( sh_cmdops_t ) );
    if ( NULL == cmdutils ) {

        // Report error
        fprintf( stdout, "\t@main(): calloc for $cmdutils failed: %s\n", strerror( errno ) );

        // Return failure
        exit( EXIT_FAILURE );

    }
    rowutils = ( sh_rowops_t * ) calloc( 1, sizeof( sh_rowops_t ) );
    if ( NULL == rowutils ) {

        // Report error
        fprintf( stdout, "\t@main(): calloc for $rowutils failed: %s\n", strerror( errno ) );

        // Return failure
        exit( EXIT_FAILURE );

    }

    // Setup $cmdutils
    cmdutils->trim = sh_cmd_trim;
    cmdutils->parse = sh_cmd_parse;
    cmdutils->inspect = sh_cmd_inspect;
    cmdutils->isvalid = sh_cmd_isvalid;
    cmdutils->isempty = sh_cmd_isempty;

    // Setup $rowutils
    rowutils->trim = sh_row_trim;
    rowutils->iscomment = sh_row_iscomment;
    rowutils->parse = sh_row_parse;
    rowutils->inspect = sh_row_inspect;
    rowutils->exec = sh_row_exec;


    /*
     * ===================================
     * Execution start
     * ===================================
     */

    // FIX @ ( CLion's delay 'd display of stderr ):
    // Redirect stderr to /dev/null ( hide stderr )
    freopen( "/dev/null", "w", stderr );

    // Clear screen
    if ( SH_MODE != 'b' ) sh_cls();

    // Print welcome screen
    sh_prt_welcome();

    // Check if file was declared but rejected
    if ( SH_MODE != 'b' && argc > 1 ) {

        // Report error
        fprintf( stdout, "\t@main(): batch mode could not be initiated: '%s' not a valid batch file\n",
                 NULL != *( argv + 1 ) ? *( argv + 1 ) : "(null)\n" );

    }

    // Init global variables
    SH_QUIT = false;
    SH_EXECUTING = false;

    // Init execution based on mode
    SH_STATUS = 'b' == SH_MODE ? mode_b( fname ) : mode_i( true );

    // Print end screen
    sh_prt_bye();

    // Free resources
    munmap( SH_SHARED_MEMORY_PTR, SHM_LEN_MAX );
    free( SH_WD_I );
    free( cmdutils );
    free( rowutils );
    if ( 'b' == SH_MODE ) free( fname );

    // Exit with success / failure
    exit( SH_STATUS ? EXIT_SUCCESS : EXIT_FAILURE );

}