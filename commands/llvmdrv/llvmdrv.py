#!/usr/bin/python
# -*- coding: iso-8859-1 -*-
'''
$PROGRAM

This program is a driver around LLVM-based compilers providing additional options.

Author: Cristiano Giuffrida (giuffrida@cs.vu.nl)
License: BSD License
Requirements: Python 2.4 or above

Syntax:
See: $PROGRAM -help

'''
import os, sys, subprocess, fcntl #@UnresolvedImport

'''
    GLOBAL VARIABLES AND DEFAULT CONFIGURATION
'''
PROGRAM = 'llvmdrv'
VERSION = 'v1.0'

LF = os.linesep

ERRORS = {
'''
  Format:
     ERROR              :       [CODE MESSAGE]
'''
     'EBUG'             :       [  1, 'Bug!' ],
     'EUNKNOWN'         :       [  2, 'Unknown error' ],
     
     'ECLPARSE'         :       [  3, 'Error parsing command line arguments' ],
     'ECLVALUE'         :       [  4, 'Bad command line argument value' ],
     'ECLINPUT'         :       [  5, 'No input files' ],
     'ERUNTIME'         :       [  6, 'Runtime error' ],
     'EBADCFILES'       :       [  7, 'Wrong number of files to compile' ]
}

#VERBOSE LEVELS
VERBOSE_LEVELS = {
    'critical'    : 1,
    'error'       : 2,
    'warning'     : 3,
    'info'        : 4,
    'debug'       : 5
} 

DEFAULT = {
    'COMPILER' :            'llvm-gcc',
    'OUTPUT_FILE' :         'a.out',
    'VERBOSE_LEVEL' :       'warning',
    'RM' :                  'rm',
    'CP' :                  'cp',
    'OPT' :                 'opt',
    'LLC' :                 'llc',
    'LLLD' :                'llvm-ld',
    'LLDIS' :               'llvm-dis',
    'ASM_EXTS' :            [ '.s', '.S'     ],
    'OBJ_EXTS' :            [ '.o', '.obj'   ],
    'LIB_EXTS' :            [ '.a'           ],
    'LLLIB_EXTS' :          [ '.bc', '.bca'  ],
    'LL_EXT' :              '.ll',
    'CCOMP_OUT_EXT' :       '.BCC',
    'COPT_OUT_EXT' :        '.bcc',
    'CLLC_OUT_EXT' :        '.bccs.s',
    'LLLLD_OUT_EXT' :       '.BCL',
    'LLLLD_SH_EXT' :        '.BCL.sh',
    'LOPT_OUT_EXT' :        '.bcl',
    'LLLC_OUT_EXT' :        '.bcls.s',
    'LAS_OUT_EXT' :         '.bclo.o',
    'STD_COMPILER_OPTIONS' :  [ '-v', '-S' ],
    'STD_LINKER_OPTIONS' :  [ '-v', '-nostartfiles', '-nodefaultlibs', '-nostdlib', '-pie', '-rdynamic', '-s', '-static', '-shared', '-shared-libgcc', '-static-libgcc', '-static-libstdc++', '-symbolic', '-export-dynamic' ],
    'STD_LLLD_OPTIONS' :    [ '-v', '-stats', '-time-passes', '-link-as-library',  '-r',  '-native',  '-native-cbe',  '-disable-inlining',  '-disable-opt',  '-disable-internalize',  '-verify-each',  '-strip-all',  '-strip-debug',  '-s',  '-S',  '-export-dynamic' ],
    'STD_DEEP_LINKER_OPTIONS' :  [ '-soname' ],
    'OPT_AGGR_PASSES' :     [ '-std-compile-opts', '-std-link-opts', '-O1', '-O2', '-O3', '-disable-inlining', '-disable-opt' ]
}

CONF = {
    'IS_PREPROCESS_ONLY' :  0,
    'INCLUDE_DIRS' :        [],
    'IS_ASSEMBLE_ONLY' :    0,
    'ASSEMBLER_OPTIONS' :   [],
    'LINKER_OPTIONS' :      [],
    'LLC_OPTIONS' :         [],
    'LLLD_OPTIONS' :        [],
    'OPT_OPTIONS' :         [],
    'PRE_COPT_OPTIONS' :    [],
    'COPT_OPTIONS' :        [],
    'LOPT_OPTIONS' :        [],
    'ASSEMBLER' :           None,
    'LINKER' :              None,
    'IS_COMPILE_ONLY' :     0,
    'COMPILER' :            DEFAULT['COMPILER'],
    'DISABLED_PASSES' :     [],
    'DISABLED_CPASSES' :    [],
    'DISABLED_LPASSES' :    [],
    'IS_DRY_RUN' :          0,
    'IS_EMIT_LLVM' :        0,
    'IS_FSYNTAX_ONLY' :     0,
    'IS_HELP' :             0,
    'OUTPUT_FILE' :         DEFAULT['OUTPUT_FILE'],
    'IS_OPT' :              0,
    'IS_SAVE_LLS' :         0,
    'IS_SAVE_TEMPS' :       0,
    'VERBOSE_LEVEL' :       DEFAULT['VERBOSE_LEVEL'],
    'IS_VERSION' :          0,
    
    #private options
    'RM' :                  DEFAULT['RM'],
    'CP' :                  DEFAULT['CP'],
    'OPT' :                 DEFAULT['OPT'],
    'LLC' :                 DEFAULT['LLC'],
    'LLLD' :                DEFAULT['LLLD'],
    'LLDIS' :               DEFAULT['LLDIS'],
    'ASM_EXTS' :            DEFAULT['ASM_EXTS'],
    'OBJ_EXTS' :            DEFAULT['OBJ_EXTS'],
    'LIB_EXTS' :            DEFAULT['LIB_EXTS'],
    'LLLIB_EXTS' :          DEFAULT['LLLIB_EXTS'],
    'LL_EXT' :              DEFAULT['LL_EXT'],
    'CCOMP_OUT_EXT' :       DEFAULT['CCOMP_OUT_EXT'],
    'COPT_OUT_EXT' :        DEFAULT['COPT_OUT_EXT'],
    'CLLC_OUT_EXT' :        DEFAULT['CLLC_OUT_EXT'],
    'LLLLD_OUT_EXT' :       DEFAULT['LLLLD_OUT_EXT'],
    'LLLLD_SH_EXT' :        DEFAULT['LLLLD_SH_EXT'],
    'LOPT_OUT_EXT' :        DEFAULT['LOPT_OUT_EXT'],
    'LLLC_OUT_EXT' :        DEFAULT['LLLC_OUT_EXT'],
    'LAS_OUT_EXT' :         DEFAULT['LAS_OUT_EXT'],
    'INPUT_FILES' :         [],
    'COMPILER_OPTIONS' :    [],
    'STD_COMPILER_OPTIONS' :  DEFAULT['STD_COMPILER_OPTIONS'],
    'STD_LINKER_OPTIONS' :  DEFAULT['STD_LINKER_OPTIONS'],
    'STD_LLLD_OPTIONS' :    DEFAULT['STD_LLLD_OPTIONS'],
    'STD_DEEP_LINKER_OPTIONS' :  DEFAULT['STD_DEEP_LINKER_OPTIONS'],
    'OPT_AGGR_PASSES' :     DEFAULT['OPT_AGGR_PASSES'],
    'IS_PRINT' :            0
}

'''
    CLASSES
'''

class EnvUtil:
    "Utility class for interacting with the environment"

    def isLogEnabled(self, level):
        if level not in VERBOSE_LEVELS:
            level = 'error'
        return (level <= CONF['VERBOSE_LEVEL'])

    def log(self, level, line, isFormatted=1):
        global LF, VERBOSE_LEVELS, CONF
        
        #default to error level if something goes wrong
        if level not in VERBOSE_LEVELS:
            level = 'error'
        
        #print if the specified level is included by configuration    
        if VERBOSE_LEVELS[level] <= VERBOSE_LEVELS[CONF['VERBOSE_LEVEL']]:
            if isFormatted:
                line = '[%s] %s%s' % (level, line, LF)
            else:
                line = '%s%s' % (line, LF)
            sys.stderr.write(line)
           
    def error(self, ekey, msg=None, level='error'):
        global ERRORS
        
        #default to unknown error if something goes wrong
        if ekey not in ERRORS:
            ekey = 'EUNKNOWN'
        
        #format the error code and message and log
        ecodeIdx = 0
        emsgIdx = 1
        ecode = ERRORS[ekey][ecodeIdx]
        emsg = ERRORS[ekey][emsgIdx]
        if msg is not None:
            emsg += '. ' + msg
        line = 'Error %d : %s' % (ecode, emsg)
        self.log(level, line)
        return ecode        
    
    def runCommand(self, args, logOnError=1, stdinData=None, isReadStdout=0, isReadStderr=0):
        verboseLevel = CONF['VERBOSE_LEVEL']
        executing = "Executing"
        if CONF['IS_DRY_RUN']:
            CONF['VERBOSE_LEVEL'] = 'info'
            executing = "Dry run for command"
        self.log('info', '%(executing)s: %(command)s' % \
            { 'executing' : executing, 'command' : ' '.join(args) })
        CONF['VERBOSE_LEVEL'] = verboseLevel
        exitCode = 0
        stdoutData = ''
        stderrData = ''
        if not CONF['IS_DRY_RUN']:
            stdin = None
            stdout = None
            stderr = None
            if stdinData is not None:
                stdin = subprocess.PIPE
            if isReadStdout:
                stdout = subprocess.PIPE
            if isReadStderr:
                stderr = subprocess.PIPE
            p = subprocess.Popen(args, 0, None, stdin, stdout, stderr)
            if stdinData or isReadStdout or isReadStderr:
                (stdoutData, stderrData) = p.communicate(stdinData)
            exitCode = p.wait()
        if exitCode != 0 and logOnError:
            self.log('error', '%(program)s exited with error code: %(code)d' % \
                { 'program' : args[0], 'code' : exitCode })
        if isReadStdout or isReadStderr:
            return exitCode, stdoutData, stderrData
        else:
            return exitCode
    
    def main(self, argv):
        global LF, CONF, VERSION
        
        #configure from the command line
        clManager = CLManager()
        unknownOpts = clManager.configure(argv[1:])
    
        #announce
        self.log('info', '%(PROGRAM)s %(VERSION)s. Running...' % { 'PROGRAM' : PROGRAM, 'VERSION' : VERSION })
        self.log('debug', 'Command was: %(LF)s%(command)s' % \
            { 'LF' : LF, 'command' : ' '.join(argv) })
        if self.isLogEnabled('debug'):
            confString = ''
            for k,v in CONF.items():
                confString += '%(LF)s\t< \'%(k)s\' : %(v)s >' % { 'LF' : LF, 'k' : k, 'v' : v }
            self.log('debug', 'Using configuration: %(conf)s' % \
                        { 'conf' : confString })
        argsUtil = ArgsUtil()
        compiler = Compiler()
        linker = Linker()
        printer = Printer()
    
        #show help and exit if needed
        if CONF['IS_HELP']:
            clManager.usage()
            sys.exit(0)
            
        #show version and exit if needed
        if CONF['IS_VERSION']:
            line = '%(PROGRAM)s %(VERSION)s %(LF)s' % { 'PROGRAM' : PROGRAM, 'VERSION' : VERSION, 'LF' : LF }
            sys.stderr.write(line)
            sys.exit(0)

        #handle print requests first
        if CONF['IS_PRINT']:
            printer.init(unknownOpts)
            args = printer.getArgs()
            ecode = self.runCommand(args)
            self.log('info', 'Done')
            sys.exit(ecode)

        #can't do much without input files
        if len(CONF['INPUT_FILES']) == 0:
            ecode = self.error('ECLINPUT')
            clManager.usage()
            sys.exit(ecode)
    
        #if we need to stop after checking the input, we are done
        if CONF['IS_FSYNTAX_ONLY']:
            self.log('info', 'Syntax checking successfully completed')
            self.log('info', 'Done')
            sys.exit(0)

        #figure out how to distribute input files
        (linkerFirstNonBitcodeInputFiles, linkerInputFiles, linkerLastNonBitcodeInputFiles, otherFiles) = linker.filterLinkerInputFiles(CONF['INPUT_FILES'])
        compilerInputFiles = otherFiles

        #if we have no linker input files, check if some of the non-bitcode input files have to be redirected to bitcode files 
        if len(linkerInputFiles) == 0 and len(linkerFirstNonBitcodeInputFiles)+len(linkerLastNonBitcodeInputFiles) > 0:
            firstNonBitcodeInputFiles = []
            lastNonBitcodeInputFiles = []
            for f in linkerFirstNonBitcodeInputFiles:
                bcFile = argsUtil.changeExtToFile(f, CONF['COPT_OUT_EXT'])
                if os.path.isfile(bcFile):
                    linkerInputFiles.append(bcFile)
                else:
                    firstNonBitcodeInputFiles.append(f)
            for f in linkerLastNonBitcodeInputFiles:
                bcFile = argsUtil.changeExtToFile(f, CONF['COPT_OUT_EXT'])
                if os.path.isfile(bcFile):
                    linkerInputFiles.append(bcFile)
                else:
                    lastNonBitcodeInputFiles.append(f)
            linkerFirstNonBitcodeInputFiles = []
            linkerFirstNonBitcodeInputFiles.extend(firstNonBitcodeInputFiles)
            linkerLastNonBitcodeInputFiles = []
            linkerLastNonBitcodeInputFiles.extend(lastNonBitcodeInputFiles)

        #figure out what we need to run
        runCompiler = (len(compilerInputFiles) > 0)
        compilerNeedsLinking = compiler.needsLinking()
        runLinker = compilerNeedsLinking or (len(linkerInputFiles)+len(linkerFirstNonBitcodeInputFiles)+len(linkerLastNonBitcodeInputFiles) > 0)
        compilerOpts = []
        cOpts = []
        linkerOpts = []
        llldOpts = []
        ldOpts = []
        if runCompiler and runLinker:
            (llldOpts, ldOpts, compilerOpts) = linker.filterLinkerOptions(unknownOpts)
        else:
            if runCompiler:
                compilerOpts = unknownOpts
            else:
                (_llldOpts, _ldOpts) = linker.filterLinkerOptions(unknownOpts, noOtherOpts=1)
                for o in _llldOpts:
                    if o not in CONF['LLLD_OPTIONS']:
                        llldOpts.append(o)
                for o in _ldOpts:
                    if o not in CONF['LINKER_OPTIONS']:
                        ldOpts.append(o)
        for o in compilerOpts:
            if o not in CONF['COMPILER_OPTIONS']:
                cOpts.append(o)
        cOpts.extend(CONF['COMPILER_OPTIONS'])
        CONF['COMPILER_OPTIONS'] = cOpts
        llldOpts.extend(CONF['LLLD_OPTIONS'])
        CONF['LLLD_OPTIONS'] = llldOpts
        ldOpts.extend(CONF['LINKER_OPTIONS'])
        CONF['LINKER_OPTIONS'] = ldOpts

        #check compiler configuration
        if runCompiler and not runLinker:
            if len(compilerInputFiles) > 1:
                ecode = self.error('EBADCFILES')
                clManager.usage()
                sys.exit(ecode)
            elif CONF['OUTPUT_FILE'] == DEFAULT['OUTPUT_FILE']:
                if CONF['IS_PREPROCESS_ONLY']:
                    CONF['OUTPUT_FILE'] = None
                elif CONF['IS_COMPILE_ONLY']:
                    CONF['OUTPUT_FILE'] = argsUtil.changeExtToFile(compilerInputFiles[0], CONF['ASM_EXTS'][0])
                else:
                    assert CONF['IS_ASSEMBLE_ONLY']
                    CONF['OUTPUT_FILE'] = argsUtil.changeExtToFile(compilerInputFiles[0], CONF['OBJ_EXTS'][0])

        #let the pass manager normalize all the options
        passManager = PassManager(runCompiler, runLinker)
        if self.isLogEnabled('debug'):
            self.log('debug', 'Normalizing...')
            passManager.normalizeOptions()
            self.log('debug', 'Normalization done, new opt options: %(LF)s\t< \'PRE_COPT_OPTIONS\' : %(PRE_COPT_OPTIONS)s >%(LF)s\t< \'COPT_OPTIONS\' : %(COPT_OPTIONS)s >%(LF)s\t< \'LOPT_OPTIONS\' : %(LOPT_OPTIONS)s >' % \
                 { 'LF' : LF, 'PRE_COPT_OPTIONS' : CONF['PRE_COPT_OPTIONS'], 'COPT_OPTIONS' : CONF['COPT_OPTIONS'], 'LOPT_OPTIONS' : CONF['LOPT_OPTIONS'] })
        else:
            passManager.normalizeOptions()

        ecode = 0
        try:
            for compilerInputFile in compilerInputFiles:
                if compilerNeedsLinking:
                    compilerOutputFile = argsUtil.changeNameToFile(CONF['OUTPUT_FILE'], compilerInputFile)
                    compilerOutputFile = argsUtil.changeExtToFile(compilerOutputFile, CONF['OBJ_EXTS'][0])
                else:
                    compilerOutputFile = CONF['OUTPUT_FILE']
                compiler.init(compilerInputFile, compilerOutputFile)
                while (1):
                    args = compiler.getNextArgs()
                    if args is None:
                        break
                    ecode = self.runCommand(args)
                    if ecode != 0:
                        raise ValueError('Exiting prematurely...')
                compiler.addLinkerInputFile(linkerInputFiles)
                compiler.addLinkerNonBitcodeInputFile(linkerFirstNonBitcodeInputFiles)
            
            if runLinker:
                linker.init(linkerFirstNonBitcodeInputFiles, linkerInputFiles, linkerLastNonBitcodeInputFiles, CONF['OUTPUT_FILE'])
                while (1):
                    args = linker.getNextArgs()
                    if args is None:
                        break
                    ecode = self.runCommand(args)
                    if ecode != 0:
                        raise ValueError('Exiting prematurely...')
        except ValueError, err:
            if ecode == 0:
                ecode = self.error('ERUNTIME', str(err))
    
        #exit
        self.log('info', 'Done')
        sys.exit(ecode)

class CLManager:
    "Command line manager"
    
    def usage(self, showArgs=1, showOpts=1):
        global LF
        
        #print usage
        print 'Usage: %(PROGRAM)s [options] [llvm-compiler options] <input files>' % { 'PROGRAM' : PROGRAM}
        if showArgs:
            s = '       Arguments: %(LF)s\
            <input files> %(LF)s\
                the input files to compile. %(LF)s\
                '
            print s % { 'LF' : LF }
        if showOpts:
            s = '       Options: %(LF)s\
            -E %(LF)s\
                Stop after the preprocessing stage, do not run the compiler. %(LF)s\
            -I %(LF)s\
                Add the directory dir to the list of directories to be searched for header files. %(LF)s\
            -S %(LF)s\
                Stop after compilation, do not assemble. %(LF)s\
            -Wa,<opt1,...,optN>,-Wa-start opt1, ..., optN -Wa-end %(LF)s\
                Pass options to assembler. %(LF)s\
            -Wl,<opt1,...,optN>,-Wl-start opt1, ..., optN -Wl-end %(LF)s\
                Pass options to linker. %(LF)s\
            -Wllc,<opt1,...,optN>,-Wllc-start opt1, ..., optN -Wllc-end %(LF)s\
                Pass options to llc (code generator). %(LF)s\
            -Wllld,<opt1,...,optN>,-Wllld-start opt1, ..., optN -Wllld-end %(LF)s\
                Pass options to llld (bitcode linker). %(LF)s\
            -Wo,<opt1,...,optN>,-Wo-start opt1, ..., optN -Wo-end %(LF)s\
                Pass options to opt. %(LF)s\
            -Woc,<opt1,...,optN>,-Wo-start opt1, ..., optN -Wo-end %(LF)s\
                Pass options to opt only during compilation. %(LF)s\
            -Wol,<opt1,...,optN>,-Wo-start opt1, ..., optN -Wo-end %(LF)s\
                Pass options to opt only during linking. %(LF)s\
            -assembler=<assembler> %(LF)s\
                The assembler to use. %(LF)s\
                Default: same as compiler. %(LF)s\
            -c %(LF)s\
                Compile and assemble, but do not link. %(LF)s\
            -compiler=<compiler> %(LF)s\
                The compiler to use. %(LF)s\
                Default: -compiler=%(COMPILER)s. %(LF)s\
            -disable-cpass=<pass_name> %(LF)s\
                Disable the specified pass during compilation. %(LF)s\
            -disable-lpass=<pass_name> %(LF)s\
                Disable the specified pass during linking. %(LF)s\
            -disable-pass=<pass_name> %(LF)s\
                Disable the specified pass during compilation and linking. %(LF)s\
            -dry-run %(LF)s\
                Only pretend to run commands. %(LF)s\
            -emit-llvm %(LF)s\
                Emit LLVM bitcode instead of native object files. %(LF)s\
            -fsyntax-only %(LF)s\
                Stop after checking the input for syntax errors. %(LF)s\
            -help %(LF)s\
                Print this help. %(LF)s\
            -uselinker=<linker> %(LF)s\
                The linker to use. %(LF)s\
                Default: same as compiler. %(LF)s\
            -o <file> %(LF)s\
                Output file name. %(LF)s\
                Default: -o %(OUTPUT_FILE)s. %(LF)s\
            -opt %(LF)s\
                Force opt. %(LF)s\
            -save-lls %(LF)s\
                Save .ll files. %(LF)s\
            -save-temps %(LF)s\
                Keep temporary files. %(LF)s\
            -verbose-level=<level> %(LF)s\
                Set the verbose level. Available options are: debug, info, warning, error, critical. %(LF)s\
                Default: -verbose-level=%(VERBOSE_LEVEL)s. %(LF)s\
            -version %(LF)s\
                Display the version of this program. %(LF)s\
                '
            print s % { 'LF' : LF, 'COMPILER' : DEFAULT['COMPILER'],
                       'OUTPUT_FILE' : DEFAULT['OUTPUT_FILE'], 'VERBOSE_LEVEL' : DEFAULT['VERBOSE_LEVEL'] }

    def configure(self, args):
        global CONF
        
        #parse command line arguments
        try:
            opts, unknownOpts, args = self.__parseArgs(args, [
                "E", "I $", "S", "Wa,", "Wl,", "Wllc,",
                "Wllld,", "Wo,", "Woc,", "Wol,", "assembler=",
                "c", "compiler=", "disable-pass=", "disable-cpass=", "disable-lpass=", "dry-run", "emit-llvm",
                "fsyntax-only", "help", "uselinker=", "o $", "opt",
                "save-lls", "save-temps", "verbose-level=", "version"],
                [".a"])
            
        except ValueError, err:
            ecode = EnvUtil.error('ECLPARSE', str(err))
            self.usage()
            sys.exit(ecode)
        
        #process args and unknown options
        CONF['INPUT_FILES'] = args
        
        #process options
        for o, a in opts:
            if o == '-E':
                CONF['IS_PREPROCESS_ONLY'] = 1
            elif o == '-I':
                CONF['INCLUDE_DIRS'].append(a)
            elif o == '-S':
                CONF['IS_COMPILE_ONLY'] = 1
            elif o == '-Wa':
                CONF['ASSEMBLER_OPTIONS'] = a
            elif o == '-Wl':
                CONF['LINKER_OPTIONS'] = a
            elif o == '-Wllc':
                CONF['LLC_OPTIONS'] = a
            elif o == '-Wllld':
                CONF['LLLD_OPTIONS'] = a
            elif o == '-Wo':
                CONF['OPT_OPTIONS'] = a
            elif o == '-Woc':
                CONF['COPT_OPTIONS'] = a
            elif o == '-Wol':
                CONF['LOPT_OPTIONS'] = a
            elif o == '-assembler':
                CONF['ASSEMBLER'] = a
            elif o == '-c':
                CONF['IS_ASSEMBLE_ONLY'] = 1
            elif o == '-compiler':
                CONF['COMPILER'] = a
            elif o == '-disable-cpass':
                CONF['DISABLED_CPASSES'].append(a)
            elif o == '-disable-lpass':
                CONF['DISABLED_LPASSES'].append(a)
            elif o == '-disable-pass':
                CONF['DISABLED_PASSES'].append(a)
            elif o == '-dry-run':
                CONF['IS_DRY_RUN'] = 1
            elif o == '-emit-llvm':
                CONF['IS_EMIT_LLVM'] = 1
            elif o == '-fsyntax-only':
                CONF['IS_FSYNTAX_ONLY'] = 1
            elif o == '-help':
                CONF['IS_HELP'] = 1
            elif o == '-uselinker':
                CONF['LINKER'] = a
            elif o == '-o':
                CONF['OUTPUT_FILE'] = a
            elif o == '-opt':
                CONF['IS_OPT'] = 1
            elif o == '-save-lls':
                CONF['IS_SAVE_LLS'] = 1
            elif o == '-save-temps':
                CONF['IS_SAVE_TEMPS'] = 1
            elif o == '-verbose-level':
                validValuesMap = { 'debug' : 'debug', 'info' : 'info', 'warning' : 'warning', 
                                   'error' : 'error', 'critical' : 'critical' }
                if validValuesMap.has_key(a):
                    CONF['VERBOSE_LEVEL'] = validValuesMap[a]
                else:
                    self.__exitOnInvalidInputValue("option", o, a)
            elif o == '-version':
                CONF['IS_VERSION'] = 1
            else:
                ecode = EnvUtil.error('EBUG', "Unhandled option: " + o, 'critical')
                sys.exit(ecode)

        #parse unknown options
        for o in unknownOpts:
            if o.startswith('-print-'):
                CONF['IS_PRINT'] = 1
                break

        #check resulting configuration
        #   - assume the user wants help if nothing has been specified
        if len(opts)+len(unknownOpts)+len(CONF['INPUT_FILES']) == 0:
            CONF['IS_HELP'] = 1
        #   - when no assembler specified, default to compiler
        if CONF['ASSEMBLER'] is None:
            CONF['ASSEMBLER'] = CONF['COMPILER']
        #   - when no linker specified, default to compiler
        if CONF['LINKER'] is None:
            CONF['LINKER'] = CONF['COMPILER']
        #   - adjust opt options
        CONF['COPT_OPTIONS'].extend(CONF['OPT_OPTIONS'])
        CONF['LOPT_OPTIONS'].extend(CONF['OPT_OPTIONS'])
        #   - adjust disabled passes
        CONF['DISABLED_CPASSES'].extend(CONF['DISABLED_PASSES'])
        CONF['DISABLED_LPASSES'].extend(CONF['DISABLED_PASSES'])
        #   - adjust assembler flags
        while ('-g' in CONF['ASSEMBLER_OPTIONS']):
            CONF['ASSEMBLER_OPTIONS'].remove("-g")

        return unknownOpts
    
    def __exitOnInvalidInputValue(self, type, opt, value):
        #log an invalid input as an error, print usage and exit
        ecode = EnvUtil.error('ECLVALUE', 'Bad %s value: %s=%s' % (type, opt, value))
        self.usage()
        sys.exit(ecode)

    def __parseArgs(self, args, validOpts, specialOptSuffixes):
        parsedArgs = []
        parsedOpts = []
        parsedUnknownOpts = []
        pendingOptName = None
        pendingOptSequence = None
        pendingOptSequenceArgs = None
        for a in args:
            if pendingOptName is not None:
                parsedOpts.append((pendingOptName, a))
                pendingOptName = None
                continue
            if a.startswith('-W'):
                wName = a[2:]
                wNameTokens = wName.split('-', 1)
                if len(wNameTokens) == 2 and ('W' + wNameTokens[0] + ',') in validOpts:
                    if wNameTokens[1] == 'start':
                        if pendingOptSequence is not None:
                            raise Exception('Unexpected start of options sequence: ' + a)
                        pendingOptSequence = wNameTokens[0]
                        pendingOptSequenceArgs = []
                        continue
                    elif wNameTokens[1] == 'end':
                        if pendingOptSequence is None or pendingOptSequence != wNameTokens[0]:
                            raise Exception('Unexpected end of options sequence: ' + a)
                        parsedName = '-W%s' % pendingOptSequence
                        isSkip = 0
                        for o in parsedOpts:
                            (name, args) = o
                            if name == parsedName:
                                args.extend(pendingOptSequenceArgs)
                                isSkip = 1
                                break
                        if not isSkip:
                            parsedOpts.append((parsedName, pendingOptSequenceArgs))
                        pendingOptSequence = None
                        pendingOptSequenceArgs = None
                        continue
            if pendingOptSequence is not None:
                pendingOptSequenceArgs.append(a)
                continue
            if a.startswith('-'):
                name = a[1:]
                #find first valid separator
                sep = '='
                commaIdx = name.find(',')
                if commaIdx != -1:
                    eqIdx = name.find('=')
                    if eqIdx == -1 or commaIdx < eqIdx:
                        sep = ','
                #parse
                nameTokens = name.split(sep, 1)
                name = nameTokens[0]
                parsedName = '-%s' % name
                if len(nameTokens) == 1:
                    nameArg =''
                else:
                    name += sep
                    if sep == ',':
                        nameArg = nameTokens[1].split(sep)
                    else:
                        nameArg = nameTokens[1]
                #see if the option is valid
                if name in validOpts:
                    isSkip = 0
                    if sep == ',':
                        for o in parsedOpts:
                            (name, args) = o
                            if name == parsedName:
                                args.extend(nameArg)
                                isSkip = 1
                                break
                    if not isSkip:
                        parsedOpts.append((parsedName, nameArg))
                    continue
                #try alternative option with an argument
                altName = "%s $" % name
                if altName in validOpts:
                    pendingOptName = parsedName
                    continue
                parsedUnknownOpts.append(a)
            else:
                for s in specialOptSuffixes:
                    if a.endswith(s):
                        parsedUnknownOpts.append(a)
                    else:
                        parsedArgs.append(a)
        if pendingOptName is not None:
            raise Exception('Missing argument for option: ' + pendingOptName)

        return parsedOpts, parsedUnknownOpts, parsedArgs

class PassManager:
    "Pass manager"
    
    MAX_CACHE_ENTRIES = 10
    CACHE_NAMED_PIPE = '/tmp/llvmdrv_cache.pipe'

    def __init__(self, runCompiler, runLinker):
        self.runCompiler = runCompiler
        self.runLinker = runLinker
        self.envUtil = EnvUtil()
        self.argsUtil = ArgsUtil()
        self.cache = None
        
    def normalizeOptions(self):
        if self.runCompiler:
            (coptionsID, coptions) = self.__lookupNormalizedOptions('COMPILER_OPTIONS', CONF['COMPILER_OPTIONS'])
            (coptOptionsID, coptOptions) = self.__lookupNormalizedOptions('COPT_OPTIONS', CONF['COPT_OPTIONS'])
            isSaveCacheEntries = 0
            if coptions is None:
                compilerPasses = self.__getPassesFromArgs(self.__getCompilerPassesArgs())
                coptions = self.__normalizeOPTOptions([], compilerPasses)
                self.__saveNormalizedOptions('COMPILER_OPTIONS', coptionsID, coptions)
                isSaveCacheEntries = 1
            if coptOptions is None:
                coptPasses = self.__getPassesFromArgs(self.__getOPTPAssesArgs(CONF['COPT_OPTIONS']))
                coptOptions = self.__normalizeOPTOptions(CONF['COPT_OPTIONS'], coptPasses)
                self.__saveNormalizedOptions('COPT_OPTIONS', coptOptionsID, coptOptions)
                isSaveCacheEntries = 1
            if isSaveCacheEntries:
                self.__saveCacheEntries()
            for p in CONF['DISABLED_CPASSES']:
                if p in coptions:
                    coptions.remove(p)
                if p in coptOptions:
                    coptOptions.remove(p)
            if not '-debug' in CONF['COPT_OPTIONS']:
                coptOptions = coptions + coptOptions
                set = {}
                coptOptions = [set.setdefault(e,e) for e in coptOptions if e not in set]
                CONF['COPT_OPTIONS'] = coptOptions
            else:
                CONF['PRE_COPT_OPTIONS'] = coptions        
                CONF['COPT_OPTIONS'] = coptOptions
            CONF['COMPILER_OPTIONS'].append('-O0')
        if self.runLinker:
            options = CONF['LOPT_OPTIONS']
            (optionsID, loptOptions) = self.__lookupNormalizedOptions('LOPT_OPTIONS', options)
            if loptOptions is None:
                loptPasses = self.__getPassesFromArgs(self.__getOPTPAssesArgs(CONF['LOPT_OPTIONS']))
                loptOptions = self.__normalizeOPTOptions(CONF['LOPT_OPTIONS'], loptPasses)
                self.__saveNormalizedOptions('LOPT_OPTIONS', optionsID, loptOptions)
                self.__saveCacheEntries()
            for p in CONF['DISABLED_LPASSES']:
                if p in loptOptions:
                    loptOptions.remove(p)
            CONF['LOPT_OPTIONS'] = loptOptions
        return
    
    def __normalizeOPTOptions(self, options, passes):
        newOptions = []
        for p in passes:
            if p not in options:
                newOptions.append(p)
        for o in options:
            if o not in CONF['OPT_AGGR_PASSES']:
                newOptions.append(o)
        set = {}
        newOptions = [set.setdefault(e,e) for e in newOptions if e not in set]
        return self.__filterUnsafeOPTOptions(newOptions)

    def __filterUnsafeOPTOptions(self, options):
        envUtil.log('debug', 'Filtering out unsafe opt options...')
        args = self.__getOPTPAssesArgs(options)
        (exitCode, _, stdErr) = self.envUtil.runCommand(args, 0, '', isReadStdout=0, isReadStderr=1)
        import re #@UnresolvedImport
        matches = re.findall('\'(-[a-zA-z0-9_-]+)\'', stdErr, re.MULTILINE)
        if exitCode == 0 or not matches or len(matches)==0:
            envUtil.log('debug', 'No unsafe opt option found.')
            return options
        for m in matches:
            if m in options:
                options.remove(m)
        envUtil.log('debug', 'Filtered out unsafe opt options: ' + ' '.join(matches))
        return options
        
    def __getCompilerPassesArgs(self):
        args = [ CONF['COMPILER'] ]
        args.extend('-c -fdebug-pass-arguments -x c -o /dev/null -'.split(' '))
        args.extend(CONF['COMPILER_OPTIONS'])
        return args
    
    def __getOPTPAssesArgs(self, options):
        if '-disable-internalize' not in options:
            options.append('-disable-internalize')
        args = [ CONF['OPT'] ]
        args.extend('-disable-output -debug-pass=Arguments'.split(' '))
        args.extend(options)
        return args        
        
    def __getPassesFromArgs(self, args):
        (exitCode, _, stdErr) = self.envUtil.runCommand(args, 1, '', isReadStdout=0, isReadStderr=1)
        import string #@UnresolvedImport
        lines = stdErr.split('\n')
        potentialOptions = []
        for l in lines:
            if l.startswith('Pass Arguments'):
                potentialOptions.extend(l.split(' '))
        if len(potentialOptions) == 0 and exitCode != 0:
            raise ValueError('Couldn\'t get passes for program: ' + args[0])
        (passes, _) = self.argsUtil.filterOptions(potentialOptions)
        return passes
    
    def __lookupNormalizedOptions(self, name, options):
        cacheEntries = self.__lookupCacheEntries(name)
        optionsID = 0
        for o in options:
            optionsID = optionsID ^ hash(o)
        optionsID = str(optionsID)
        normalizedOptions = None
        for e in cacheEntries:
            keyValue = e.split(':', 1)
            if optionsID == keyValue[0]:
                normalizedOptions = keyValue[1].split(' ')
                break
        return optionsID, normalizedOptions

    def __saveNormalizedOptions(self, name, optionsID, normalizedOptions):
        cacheEntries = self.__lookupCacheEntries(name)
        newCacheEntry = '%s:%s' % (optionsID, ' '.join(normalizedOptions))
        cacheEntries.append(newCacheEntry)
        if len(cacheEntries) > PassManager.MAX_CACHE_ENTRIES:
            envUtil.log('debug', 'Cache named %(NAME)s has reached the maximum number of %(NUM)d entries, removing the first entry: %(FIRST)s' % \
                        { 'NAME' : name, 'NUM' : PassManager.MAX_CACHE_ENTRIES, 'FIRST' : cacheEntries[0] })
            del cacheEntries[0]
        self.cache[name] = cacheEntries
        envUtil.log('debug', 'Saved cache VARIABLE \'%(VAR)s=%(VALUE)s\'' % \
            { 'VAR' : name, 'VALUE' : '|'.join(cacheEntries) })
        return
    
    def __lookupCacheEntries(self, name):
        if self.cache is None:
            self.cache = {}
            pipe = None
            try:
                pipe = open(PassManager.CACHE_NAMED_PIPE, 'r')
                fcntl.fcntl(pipe, fcntl.F_SETFL, os.O_NONBLOCK)
                lines = pipe.readlines()
            except IOError:
                lines = []
            for l in lines:
                tokens = l.split('=',1)
                if len(tokens) != 2:
                    continue
                self.cache[tokens[0]] = tokens[1].replace('\n', '').split('|')
            if pipe:
                pipe.close()
        cacheEntries = []
        if name in self.cache:
            cacheEntries = self.cache[name]
        envUtil.log('debug', 'Looked up cache VARIABLE \'%(VAR)s=%(VALUE)s\'' % \
            { 'VAR' : name, 'VALUE' : '|'.join(cacheEntries) })
        return cacheEntries
    
    def __saveCacheEntries(self):
        assert self.cache is not None
        try:
            pipe = open(PassManager.CACHE_NAMED_PIPE, 'w')
        except IOError:
            os.mkfifo(PassManager.CACHE_NAMED_PIPE)
            pipe = open(PassManager.CACHE_NAMED_PIPE, 'w')
        for k,v in self.cache.items():
            line = '%(k)s=%(v)s\n' % { 'k' : k, 'v' : '|'.join(v) }
            pipe.write(line)
        pipe.close()

class ArgsUtil:
    "Utility class for manipulating arguments"

    def changeExtToFile(self, file, ext, isAppend=0):
        if isAppend:
            newFile = "%s%s" % (file, ext)
        else:
            (fileRoot, _) = os.path.splitext(file)
            newFile = "%s%s" % (fileRoot, ext)
        return newFile
    
    def changeNameToFile(self, file, fileFrom):
        (fileRoot, fileExt) = os.path.splitext(file)
        (fileFromRoot, _) = os.path.splitext(fileFrom)
        (filePath, _) = os.path.split(fileRoot)
        (_, fileFromName) = os.path.split(fileFromRoot)
        if filePath == '' and not os.path.isabs(file):
            newFile = "%s%s" % (fileFromName, fileExt)
        else:
            newFile = "%s/%s%s" % (filePath, fileFromName, fileExt)
        return os.path.normpath(newFile)
  
    def getGenericToolArgs(self, tool, inputFiles, outputFile, options, isOptionsAtTheEnd=0):
        set = {}
        options = [set.setdefault(e,e) for e in options if e not in set]
        args = []
        args.append(tool)
        if not isOptionsAtTheEnd:
            args.extend(options)
        if outputFile is not None:
            args.append('-o')
            args.append(outputFile)
        args.extend(inputFiles)
        if isOptionsAtTheEnd:
            args.extend(options)
        return args

    def getBitcodeDisArgsMap(self, bitcodeFiles):
        argsMap = []
        for bitcodeFile in bitcodeFiles:
            args = self.getGenericToolArgs(CONF['LLDIS'], [ bitcodeFile ], self.changeExtToFile(bitcodeFile, CONF['LL_EXT'], isAppend=1), [])
            argsMap.append(args)
        return argsMap

    def getCleanupArgs(self, cleanupFiles):
        return self.getGenericToolArgs(CONF['RM'], cleanupFiles, None, [ '-f' ])  

    def getCopyArgs(self, inputFiles, outputFile):
        return [ CONF['CP'], inputFiles[0], outputFile ]
    
    def getIncludeDirArgs(self, includeDirs):
        args = []
        for d in includeDirs:
            args.append('-I')
            args.append(d)
        return args
        
    def removeDuplicatedOptions(self, args):
        newArgs = []
        newOptions = []
        argNext = 0
        i=0
        for a in args:
            if argNext:
                argNext = 0
                newArgs.append(a)
                i += 1
                continue
            assert a[0] == '-'
            if i+1 < len(args) and args[i+1][0] != '-':
                argNext = 1
                newArgs.append(a)
            else:
                newOptions.append(a)
            i += 1
        set = {}
        newOptions = [set.setdefault(e,e) for e in newOptions if e not in set]
        return newArgs + newOptions
        
    def filterOptions(self, args):
        options = []
        otherArgs = []
        for a in args:
            if a.startswith('-'):
                options.append(a)
            else:
                otherArgs.append(a)
        return options, otherArgs

class Compiler:
    "A compiler"

    COMPILE =       0;
    PRE_OPT =       1;
    RUN_OPT =       2;
    RUN_LLC =       3;
    ASSEMBLE =      4;
    FINALIZE =      5;
    DONE =          6;

    def init(self, inputFile, outputFile):
        assert inputFile and inputFile != ''
        assert CONF['IS_PREPROCESS_ONLY'] or (outputFile and outputFile != '')
        self.isAsmInputFile = os.path.splitext(inputFile)[1] in CONF['ASM_EXTS']
        if CONF['IS_PREPROCESS_ONLY']+CONF['IS_ASSEMBLE_ONLY']+CONF['IS_COMPILE_ONLY'] > 1:
            raise ValueError('Can only specify one of -E, -S, -c')
        if self.isAsmInputFile:
            if (CONF['IS_PREPROCESS_ONLY'] or CONF['IS_COMPILE_ONLY']):
                raise ValueError('Cannot specify one of -E, -S with an assembly file')
            if CONF['IS_EMIT_LLVM']:
                raise ValueError('Cannot emit LLVM code with an assembly file')
        if CONF['IS_PREPROCESS_ONLY'] and CONF['IS_EMIT_LLVM']:
            raise ValueError('Cannot emit LLVM code after preprocessing')

        self.argsMap = {
            Compiler.COMPILE :     self.__getCompilerArgs,
            Compiler.PRE_OPT :     self.__getPreOPTArgs,
            Compiler.RUN_OPT :     self.__getOPTArgs,
            Compiler.RUN_LLC :     self.__getLLCArgs,
            Compiler.ASSEMBLE :    self.__getAssemblerArgs,
            Compiler.FINALIZE :    self.__getFinalizeArgs,
            Compiler.DONE :        self.__getNoArgs
        };
        self.isSkipOpt = not CONF['IS_OPT'] and len(CONF['COPT_OPTIONS']) == 0
        self.inputFiles = [ inputFile ]
        self.outputFile = outputFile
        self.argsUtil = ArgsUtil()
        self.reset()
    
    def reset(self):
        self.intermediateFiles = []
        self.bitcodeFiles = []
        self.isFinalizing = 0
        self.finalizeArgs = []
        if self.isAsmInputFile:
            self.nextState = Compiler.ASSEMBLE
        else:
            self.nextState = Compiler.COMPILE
        self.nextInputFiles = self.inputFiles

    def getNextArgs(self):
        state = self.nextState
        if self.nextState != Compiler.DONE:
            self.nextState += 1
        args = self.argsMap[state]()
        return args
    
    def needsLinking(self):
        return not (CONF['IS_PREPROCESS_ONLY'] or CONF['IS_COMPILE_ONLY'] or CONF['IS_ASSEMBLE_ONLY'])

    def addLinkerInputFile(self, linkerInputFiles):
        if not self.isAsmInputFile and self.needsLinking():
            linkerInputFiles.append(self.argsUtil.changeExtToFile(self.outputFile, CONF['COPT_OUT_EXT']))

    def addLinkerNonBitcodeInputFile(self, linkerNonBitcodeInputFiles):
        if self.isAsmInputFile and self.needsLinking():
            linkerNonBitcodeInputFiles.append(self.outputFile)

    def __getCompilerArgs(self):
        myInputFiles = self.nextInputFiles
        if CONF['IS_PREPROCESS_ONLY']:
            myOutputFile = self.outputFile
            self.nextState = Compiler.FINALIZE
        else:
            myOutputFile = self.argsUtil.changeExtToFile(self.outputFile, CONF['CCOMP_OUT_EXT'])
            self.intermediateFiles.append(myOutputFile)
            self.bitcodeFiles.append(myOutputFile)
        self.nextInputFiles = [myOutputFile]
        compilerOptions = []
        compilerOptions.extend(CONF['COMPILER_OPTIONS'])
        if CONF['IS_PREPROCESS_ONLY']:
            compilerOptions.append('-E')
        else:
            compilerOptions.append('-c')
            compilerOptions.append('-emit-llvm')
        args = self.argsUtil.getGenericToolArgs(CONF['COMPILER'], myInputFiles, myOutputFile, compilerOptions)
        return [args[0]] + self.argsUtil.getIncludeDirArgs(CONF['INCLUDE_DIRS']) + args[1:] 
     
    def __getPreOPTArgs(self):
        if len(CONF['PRE_COPT_OPTIONS']) == 0:
            return self.getNextArgs()
        myInputFiles = self.nextInputFiles
        myOutputFile = self.argsUtil.changeExtToFile(self.outputFile, '.pre' + CONF['COPT_OUT_EXT'])
        self.bitcodeFiles.append(myOutputFile)
        self.nextInputFiles = [myOutputFile]
        options = [ '-disable-internalize' ]
        options.extend(CONF['PRE_COPT_OPTIONS'])
        return self.argsUtil.getGenericToolArgs(CONF['OPT'], myInputFiles, myOutputFile, options)

    def __getOPTArgs(self):
        myInputFiles = self.nextInputFiles
        if CONF['IS_EMIT_LLVM'] and not self.needsLinking():
            myOutputFile = self.outputFile
            self.nextState = Compiler.FINALIZE
        else:
            myOutputFile = self.argsUtil.changeExtToFile(self.outputFile, CONF['COPT_OUT_EXT'])
        self.bitcodeFiles.append(myOutputFile)
        self.nextInputFiles = [myOutputFile]
        if self.isSkipOpt:
            return self.argsUtil.getCopyArgs(myInputFiles, myOutputFile)
        else:
            options = [ '-disable-internalize' ]
            options.extend(CONF['COPT_OPTIONS'])
            return self.argsUtil.getGenericToolArgs(CONF['OPT'], myInputFiles, myOutputFile, options)
        
    def __getLLCArgs(self):
        myInputFiles = self.nextInputFiles
        if CONF['IS_COMPILE_ONLY']:
            myOutputFile = self.outputFile
            self.nextState = Compiler.FINALIZE
        else:
            myOutputFile = self.argsUtil.changeExtToFile(self.outputFile, CONF['CLLC_OUT_EXT'])
            self.intermediateFiles.append(myOutputFile)
        self.nextInputFiles = [myOutputFile]
        llcOptions = []
        llcOptions.extend(CONF['LLC_OPTIONS'])
        llcOptions.extend([ '-filetype=asm', '-O1']) #xxx should be -O0 but there is an LLVM bug preventing this
        return self.argsUtil.getGenericToolArgs(CONF['LLC'], myInputFiles, myOutputFile, llcOptions)

    def __getAssemblerArgs(self):
        myInputFiles = self.nextInputFiles
        myOutputFile = self.outputFile
        self.nextInputFiles = [myOutputFile]
        assemblerOptions = [ '-D__ASSEMBLY__' ]
        assemblerOptions.extend(CONF['ASSEMBLER_OPTIONS'])
        assemblerOptions.append('-c')
        args = self.argsUtil.getGenericToolArgs(CONF['ASSEMBLER'], myInputFiles, myOutputFile, assemblerOptions)
        return [args[0]] + self.argsUtil.getIncludeDirArgs(CONF['INCLUDE_DIRS']) + args[1:]

    def __getFinalizeArgs(self):
        if not self.isFinalizing:
            self.isFinalizing = 1
            self.finalizeArgs = []
            if CONF['IS_SAVE_LLS']:
                self.finalizeArgs.extend(self.argsUtil.getBitcodeDisArgsMap(self.bitcodeFiles))
            if not CONF['IS_SAVE_TEMPS'] and len(self.intermediateFiles) > 0:
                self.finalizeArgs.append(self.argsUtil.getCleanupArgs(self.intermediateFiles))
        if len(self.finalizeArgs) == 0:
            return None
        args = self.finalizeArgs[0]
        del self.finalizeArgs[0]
        self.nextState -= 1 #repeat FINALIZE
        return args

    def __getNoArgs(self):
        return None

class Linker:
    "A linker"

    LLLD =          0;
    RUN_OPT =       1;
    RUN_LLC =       2;
    ASSEMBLE =      3;
    LINK =          4;
    FINALIZE =      5;
    DONE =          6;

    def init(self, firstNonBitcodeInputFiles, inputFiles, lastNonBitcodeInputFiles, outputFile):
        assert len(inputFiles)+len(firstNonBitcodeInputFiles)+len(lastNonBitcodeInputFiles) > 0
        assert outputFile and outputFile != ''
        if CONF['IS_PREPROCESS_ONLY']+CONF['IS_ASSEMBLE_ONLY']+CONF['IS_COMPILE_ONLY'] > 0:
            raise ValueError('Cannot pass -E, -S, -c to the linker')

        self.argsMap = {
            Linker.LLLD :          self.__getLLLDArgs,
            Linker.RUN_OPT :       self.__getOPTArgs,
            Linker.RUN_LLC :       self.__getLLCArgs,
            Linker.ASSEMBLE :      self.__getAssemblerArgs,
            Linker.LINK :          self.__getLinkerArgs,
            Linker.FINALIZE :      self.__getFinalizeArgs,
            Linker.DONE :          self.__getNoArgs
        };
        self.isSkipOpt = not CONF['IS_OPT'] and len(CONF['LOPT_OPTIONS']) == 0
        self.inputFiles = inputFiles
        self.firstNonBitcodeInputFiles = firstNonBitcodeInputFiles
        self.lastNonBitcodeInputFiles = lastNonBitcodeInputFiles
        self.outputFile = outputFile
        self.argsUtil = ArgsUtil()
        self.reset()
    
    def reset(self):
        self.intermediateFiles = []
        self.bitcodeFiles = []
        self.isFinalizing = 0
        self.finalizeArgs = []
        self.nextInputFiles = self.inputFiles
        if len(self.inputFiles) > 0:
            self.nextState = Linker.LLLD
        else:
            self.nextState = Linker.LINK

    def getNextArgs(self):
        state = self.nextState
        if self.nextState != Linker.DONE:
            self.nextState += 1
        args = self.argsMap[state]()
        return args

    def filterLinkerInputFiles(self, inputFiles):
        linkerInputFiles = []
        linkerFirstNonBitcodeInputFiles = []
        linkerLastNonBitcodeInputFiles = []
        otherFiles = []
        linkerExts = []
        linkerExts.extend([ CONF['CCOMP_OUT_EXT'], CONF['COPT_OUT_EXT'] ])
        linkerExts.extend(CONF['LLLIB_EXTS'])
        linkerNonBitcodeExts = []
        linkerNonBitcodeExts.extend(CONF['OBJ_EXTS'])
        linkerNonBitcodeExts.extend(CONF['LIB_EXTS'])
        for f in inputFiles:
            ext = os.path.splitext(f)[1]
            if ext in linkerExts:
                linkerInputFiles.append(f)
            elif ext in linkerNonBitcodeExts:
                if len(linkerInputFiles) == 0:
                    linkerFirstNonBitcodeInputFiles.append(f)
                else:
                    linkerLastNonBitcodeInputFiles.append(f)
            else:
                otherFiles.append(f) 
        return linkerFirstNonBitcodeInputFiles, linkerInputFiles, linkerLastNonBitcodeInputFiles, otherFiles

    def filterLinkerOptions(self, opts, noOtherOpts=0):
        llldOpts = []
        ldOpts = []
        otherOpts = []
        argsUtil = ArgsUtil()
        for o in opts:
            if o.startswith('-l') or o.startswith('-L'):
                llldOpts.append(o)
                ldOpts.append(o)
            elif o.endswith('.a'):
                bcFile = argsUtil.changeExtToFile(o, CONF['LLLIB_EXTS'][0])
                if os.path.isfile(bcFile):
                    llldOpts.append(bcFile)
                else:
                    llldOpts.append(o)
                ldOpts.append(o)
            else:
                optFound = 0
                if o in CONF['STD_LINKER_OPTIONS']:
                    ldOpts.append(o)
                    optFound = 1
                if o in CONF['STD_LLLD_OPTIONS']:
                    llldOpts.append(o)
                    optFound = 1
                if optFound and o in CONF['STD_COMPILER_OPTIONS']:
                    otherOpts.append(o)
                if not optFound:
                    if noOtherOpts:
                        ldOpts.append(o)
                    else:
                        otherOpts.append(o)
        if noOtherOpts:
            return llldOpts, ldOpts
        return llldOpts, ldOpts, otherOpts

    def __getLLLDArgs(self):
        myInputFiles = self.nextInputFiles
        myShellFile = self.argsUtil.changeExtToFile(self.outputFile, CONF['LLLLD_SH_EXT'])
        myOutputFile = self.argsUtil.changeExtToFile(self.outputFile, CONF['LLLLD_OUT_EXT'])
        self.intermediateFiles.append(myShellFile)
        self.intermediateFiles.append(myOutputFile)
        self.bitcodeFiles.append(myOutputFile)
        self.nextInputFiles = [myOutputFile]
        llldOptions = [ '-disable-internalize', '-disable-opt' ]
        llldOptions.extend(CONF['LLLD_OPTIONS'])
        llldOptions.append('-b')
        llldOptions.append(myOutputFile)
        return self.argsUtil.getGenericToolArgs(CONF['LLLD'], myInputFiles, myShellFile, llldOptions, isOptionsAtTheEnd=1)
        
    def __getOPTArgs(self):
        myInputFiles = self.nextInputFiles
        if CONF['IS_EMIT_LLVM']:
            myOutputFile = self.outputFile
            self.nextState = Linker.FINALIZE
        else:
            myOutputFile = self.argsUtil.changeExtToFile(self.outputFile, CONF['LOPT_OUT_EXT'])
            self.intermediateFiles.append(myOutputFile)
        self.bitcodeFiles.append(myOutputFile)
        self.nextInputFiles = [myOutputFile]
        if self.isSkipOpt:
            return self.argsUtil.getCopyArgs(myInputFiles, myOutputFile)
        else:
            options = [ '-disable-internalize' ]
            options.extend(CONF['LOPT_OPTIONS'])
            return self.argsUtil.getGenericToolArgs(CONF['OPT'], myInputFiles, myOutputFile, options)
        
    def __getLLCArgs(self):
        myInputFiles = self.nextInputFiles
        myOutputFile = self.argsUtil.changeExtToFile(self.outputFile, CONF['LLLC_OUT_EXT'])
        self.intermediateFiles.append(myOutputFile)
        self.nextInputFiles = [myOutputFile]
        llcOptions = []
        llcOptions.extend(CONF['LLC_OPTIONS'])
        llcOptions.extend([ '-filetype=asm', '-O1']) #xxx should be -O0 but there is an LLVM bug preventing this
        return self.argsUtil.getGenericToolArgs(CONF['LLC'], myInputFiles, myOutputFile, llcOptions)

    def __getAssemblerArgs(self):
        myInputFiles = self.nextInputFiles
        myOutputFile = self.argsUtil.changeExtToFile(self.outputFile, CONF['LAS_OUT_EXT'])
        self.intermediateFiles.append(myOutputFile)
        self.nextInputFiles = [myOutputFile]
        assemblerOptions = [ '-D__ASSEMBLY__' ]
        assemblerOptions.extend(CONF['ASSEMBLER_OPTIONS'])
        assemblerOptions.append('-c')
        args = self.argsUtil.getGenericToolArgs(CONF['ASSEMBLER'], myInputFiles, myOutputFile, assemblerOptions)
        return [args[0]] + self.argsUtil.getIncludeDirArgs(CONF['INCLUDE_DIRS']) + args[1:]

    def __getLinkerArgs(self):
        myInputFiles = []
        myInputFiles.extend(self.firstNonBitcodeInputFiles)
        myInputFiles.extend(self.nextInputFiles)
        myInputFiles.extend(self.lastNonBitcodeInputFiles)
        myOutputFile = self.outputFile
        self.nextInputFiles = [myOutputFile]
        linkerOptions = []
        wasStdDeepLinkerOptWithArg = 0
        for o in CONF['LINKER_OPTIONS']:
            if wasStdDeepLinkerOptWithArg:
                wasStdDeepLinkerOptWithArg = 0
                linkerOptions.append('-Wl,' + o)
            elif o in CONF['STD_DEEP_LINKER_OPTIONS']:
                linkerOptions.append('-Wl,' + o)
                wasStdDeepLinkerOptWithArg = 1
            else:
                linkerOptions.append(o)
        return self.argsUtil.getGenericToolArgs(CONF['LINKER'], myInputFiles, myOutputFile, linkerOptions, isOptionsAtTheEnd=1)
    
    def __getFinalizeArgs(self):
        if not self.isFinalizing:
            self.isFinalizing = 1
            self.finalizeArgs = []
            if CONF['IS_SAVE_LLS']:
                self.finalizeArgs.extend(self.argsUtil.getBitcodeDisArgsMap(self.bitcodeFiles))
            if not CONF['IS_SAVE_TEMPS'] and len(self.intermediateFiles) > 0:
                self.finalizeArgs.append(self.argsUtil.getCleanupArgs(self.intermediateFiles))
        if len(self.finalizeArgs) == 0:
            return None
        args = self.finalizeArgs[0]
        del self.finalizeArgs[0]
        self.nextState -= 1 #repeat FINALIZE
        return args

    def __getNoArgs(self):
        return None

class Printer:
    "A printer"

    def init(self, options):
        self.options = options
        self.argsUtil = ArgsUtil()

    def getArgs(self):
        return self.argsUtil.getGenericToolArgs(CONF['COMPILER'], [], None, self.options)

#ENTRY POINT
if __name__ == "__main__":
    envUtil = EnvUtil()
    sys.exit(envUtil.main(sys.argv))

