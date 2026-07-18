const vscode = require('vscode');
const { LanguageClient, TransportKind } = require('vscode-languageclient');
const vscode_languageclient = require('vscode-languageclient');
const fs = require('fs');
const path = require('path');

let client;

function activate(context) {
    const exe = process.platform === 'win32' ? 'toy-lsp.exe' : 'toy-lsp';
    const configuredPath = vscode.workspace
        .getConfiguration('toy')
        .get('lsp.path', '');
    const bundledPath = path.join(context.extensionPath, 'bin', exe);
    const serverPath = configuredPath ||
        (fs.existsSync(bundledPath) ? bundledPath : exe);

    const serverOptions = {
        command: serverPath,
        transportKind: TransportKind.stdio
    };

    const outputChannel = vscode.window.createOutputChannel('Toy LSP');

    const clientOptions = {
        documentSelector: [
            { language: 'toy', scheme: 'file' },
            { language: 'toy', scheme: 'untitled' }
        ],
        revealOutputChannelOn: vscode_languageclient.RevealOutputChannelOn.Never,
        errorHandler: {
            error: (message, count) => {
                outputChannel.appendLine(`Error: ${message}`);
                return { action: vscode_languageclient.ErrorAction.Consume };
            },
            closed: () => {
                return { action: vscode_languageclient.CloseAction.DoNotRestart };
            }
        }
    };

    client = new LanguageClient('toy', 'Toy LSP', serverOptions, clientOptions);
    client.start();

    context.subscriptions.push(client, outputChannel);
}

function deactivate() {
    if (client) {
        return client.stop();
    }
    return undefined;
}

exports.activate = activate;
exports.deactivate = deactivate;
