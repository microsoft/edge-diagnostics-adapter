/*---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 *--------------------------------------------------------*/

const gulp = require('gulp');
const path = require('path');
const ts = require('gulp-typescript');
const log = require('gulp-util').log;
const typescript = require('typescript');
const sourcemaps = require('gulp-sourcemaps');
const mocha = require('gulp-mocha');
const tslint = require('gulp-tslint');
const msbuild = require("gulp-msbuild");
const argv = require('yargs').argv;
var exec = require('child_process').exec;

var sources = [
    'src',
    'lib',
    'test',
    'typings/main'
].map(function(tsFolder) { return tsFolder + '/**/*.ts'; });

var lintSources = [
    'src',
    'lib',
    'test'
].map(function(tsFolder) { return tsFolder + '/**/*.ts'; });

var deploySources = [
    'src/**/*.html',
    'src/**/*.css',
    'src/**/*.json'
];

var nativeSources = [
    'native/Common',
    'native/DebuggerCore',
    'native/Proxy',
].map(function(tsFolder) { return tsFolder + '/*.vcxproj'; });

var projectConfig = {
    noImplicitAny: false,
    target: 'ES5',
    module: 'commonjs',
    declaration: true,
    typescript: typescript,
    moduleResolution: "node"
};

gulp.task('buildtypescript', function () {
	return gulp.src(sources, { base: '.' }) 
        .pipe(sourcemaps.init()) 
        .pipe(ts(projectConfig)).js 
        .pipe(sourcemaps.write('.', { includeContent: false, sourceRoot: 'file:///' + __dirname })) 
        .pipe(gulp.dest('out'));
});

gulp.task('buildscript', ['buildtypescript'], function() {
    return gulp.src(deploySources, { base: '.' })
            .pipe(gulp.dest('out'));
});


function getNativeBuildOptions() {
    const target = (argv.rebuild ? 'Rebuild' : 'Build');
    const config = (argv.debug ? 'Debug' : 'Release');
    const arch = (argv.x64 ? 'x64' : 'Win32');
    const verbose = (argv.verbose ? '' : 'ErrorsOnly;WarningsOnly')
    const outDir = 'out/native/' + config + '/' + arch + '/';
    const outArch =  (argv.x64 ? '64' : '');
    
    return {
        target: target,
        config: config,
        arch: arch,
        verbose: verbose,
        outDir: outDir,
        outArch: outArch
    }
}

gulp.task('buildnativeprojects', function() {
    const opts = getNativeBuildOptions();
    return gulp.src('native/Proxy/Proxy.vcxproj', { base: '.' })
            .pipe(msbuild({
                targets: [opts.target],
                configuration: opts.config,
                properties: { Platform: opts.arch },
                stdout: true,
                stderr: true,
                logCommand: true,
                toolsVersion: 14,
                consoleLoggerParameters: opts.verbose
            }));
});

gulp.task('buildnativeaddon', ['buildnativeprojects'], function(done) {
    const opts = getNativeBuildOptions();
    const arch = opts.arch == "Win32" ? "ia32" : "x64";
    const gypPath = __dirname + "/node_modules/.bin/node-gyp";
    
    return exec('cd native/Addon && ' + gypPath + ' clean configure build --arch=' + arch + " --module_arch=" + opts.outArch, function (err, stdout, stderr) {
        console.log(stdout);
        console.log(stderr);
        done(err);
    });
});

gulp.task('buildnative', ['buildnativeaddon'], function() {
    const opts = getNativeBuildOptions();
    return gulp.src([opts.outDir + 'Proxy' + opts.outArch + '.dll'], { base: opts.outDir })
            .pipe(gulp.dest('out/lib'));
});

gulp.task('default', ['buildnative']);

gulp.task('buildall', ['buildscript', 'buildnative']);

gulp.task('build', ['buildall']);

gulp.task('watch', ['buildscript'], function() {
    log('Watching build sources...');
    return gulp.watch(sources, ['buildscript']);
});

gulp.task('tslint', function() { 
    return gulp.src(lintSources, { base: '.' })
         .pipe(tslint()) 
         .pipe(tslint.report('verbose')); 
}); 

function test() {
    return gulp.src('out/test/**/*.test.js', { read: false })
        .pipe(mocha({ ui: 'tdd' }))
        .on('error', function(e) {
            log(e ? e.toString() : 'error in test task!');
            this.emit('end');
        });
}

gulp.task('build-test', ['build'], test);
gulp.task('test', test);

gulp.task('watch-build-test', ['build', 'build-test'], function() {
    return gulp.watch(sources, ['build', 'build-test']);
});
