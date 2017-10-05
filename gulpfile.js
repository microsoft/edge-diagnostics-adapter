/* ---------------------------------------------------------
 * Copyright (C) Microsoft Corporation. All rights reserved.
 * --------------------------------------------------------*/

const gulp = require('gulp');
const fs = require('fs');
const path = require('path');
const gutil = require('gulp-util');
const log = gutil.log;
const colors = gutil.colors;
const mocha = require('gulp-mocha');
const msbuild = require('gulp-msbuild');
const argv = require('yargs').argv;
const exec = require('child_process').exec;
const gulpSequence = require('gulp-sequence');

const sources = [
    'src',
    'lib',
    'test',
    'typings'
].map(function (tsFolder) {
    return tsFolder + '/**/*.ts';
});

function logExec(stdout, stderr) {
    if (stdout) {
        stdout.toString().trim()
            .split(/\r?\n/)
            .forEach((line) => {
                log(line);
            });
    }

    if (stderr) {
        stderr.toString().trim()
            .split(/\r?\n/)
            .forEach((line) => {
                log(colors.red(line));
            });
    }
}

function getNativeBuildOptions() {
    // const target = (argv.rebuild ? 'Rebuild' : 'Build');
    const config = 'Release';
    const arch = process.arch === 'x64' ? 'x64' : 'Win32';
    // const verbose = (argv.verbose ? '' : 'ErrorsOnly;WarningsOnly')
    const outDir = 'out/native/' + config + '/' + arch + '/';
    const networkOutDir = 'out/native/network/' + config + '/' + arch + '/';
    const outArch = arch === 'x64' ? '64' : '';

    return {
        target: 'Build',
        config: config,
        arch: arch,
        verbose: 'ErrorsOnly;WarningsOnly',
        outDir: outDir,
        networkOutDir: networkOutDir,
        outArch: outArch
    };
}

gulp.task('buildnativeproxy', function () {
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

gulp.task('buildnativenetworkproxy', function () {
    const opts = getNativeBuildOptions();

    return gulp.src('native/NetworkProxy/NetworkProxy.vcxproj', { base: '.' })
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

gulp.task('buildnativeprojects', ['buildnativeproxy', 'buildnativenetworkproxy']);

gulp.task('buildnativeaddon', ['buildnativeprojects'], function (done) {
    const opts = getNativeBuildOptions();
    const arch = opts.arch === 'Win32' ? 'ia32' : 'x64';
    const gypPath = path.join(__dirname, '/node_modules/.bin/node-gyp');

    return exec('cd native/Addon && ' + gypPath + ' clean configure build --arch=' + arch, function (err, stdout, stderr) {
        logExec(stdout);
        logExec(stderr); // node-gyp sends info through stderr and we don't want to treat it as error
        done(err);
    });
});

gulp.task('buildnative', gulpSequence(['buildnativeaddon'], ['copyproxy', 'copynetworkproxy']));

gulp.task('copyproxy', function () {
    const opts = getNativeBuildOptions();

    return gulp.src([
        opts.outDir + 'Proxy.dll',
        opts.outDir + 'Proxy.pdb'
    ], { base: opts.outDir })
        .pipe(gulp.dest('out/lib'));
});

gulp.task('copynetworkproxy', function () {
    const opts = getNativeBuildOptions();

    return gulp.src(
        [
            opts.networkOutDir + 'NetworkProxy.exe',
            opts.networkOutDir + 'NetworkProxy.pdb'
        ])
        .pipe(gulp.dest('out/lib'));
});

gulp.task('default', ['buildnative']);

function test() {
    return gulp.src('out/test/**/*.test.js', { read: false })
        .pipe(mocha({ ui: 'tdd' }))
        .on('error', function (e) {
            log(e ? e.toString() : 'error in test task!');
            this.emit('end'); // eslint-disable-line no-invalid-this
        });
}

gulp.task('build-test', ['build'], test);
gulp.task('test', test);

gulp.task('watch-build-test', ['build', 'build-test'], function () {
    return gulp.watch(sources, ['build', 'build-test']);
});

gulp.task('copypasta', function (done) {
    // Recursively copy out folder to another folder
    if (!argv.outDir || !fs.existsSync(argv.outDir)) { // eslint-disable-line no-sync
        log(colors.red('Usage: gulp copypasta --outDir <path>'));

        return;
    }

    exec(`xcopy out ${argv.outDir} /S /Y /D`, function (err, stdout, stderr) {
        logExec(stdout, stderr);
        done(err);
    });
});
