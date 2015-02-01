#!/usr/bin/env python

# Test the that misuses of the GC API cause compiler errors.

import os
import subprocess
import tempfile
import unittest

class TestCompileErrors(unittest.TestCase):
    compiler = "g++"
    flags = ["-Wall", "-Werror", "--std=c++11"]

    prelude = """
    struct TestCell : public Cell
    {
        virtual void traceChildren(Tracer& t) {
            for (auto i = children_.begin(); i != children_.end(); ++i)
                gc::trace(t, &(*i));
        }

        virtual size_t size() const { return sizeof(*this); }

        virtual void print(ostream& s) const {}

        void addChild(TestCell* cell) {
            children_.push_back(cell);
        }

      private:
        vector<TestCell*> children_;
    };
    """

    epilog = """
    int main(int argc, char* argv[]) {
        test();
        return 0;
    }
    """

    def setUp(self):
        scriptDir = os.path.dirname(os.path.realpath(__file__))
        self.srcPath = os.path.realpath(os.path.join(scriptDir, "..", "src"))

    def writeSource(self, source):
        (handle, path) = tempfile.mkstemp(".cpp", "gc-api-test", text = True)
        f = os.fdopen(handle, "w")
        f.write("#include \"%s/gc.h\"\n" % self.srcPath)
        f.write(self.prelude + "\n")
        f.write(source + "\n")
        f.write(self.epilog + "\n")
        f.close()
        return path

    def runCommand(self, command):
        proc = subprocess.Popen(command,
                                stdout = subprocess.PIPE,
                                stderr = subprocess.STDOUT)
        output = ""
        while proc.poll() == None:
            output += proc.stdout.readline()
        return proc.returncode, output

    def compileSource(self, sourcePath, outputPath, debug, link):
        debugDefine = "DEBUG" if debug else "NDEBUG"
        command = [self.compiler] + self.flags
        if not link:
            command.append('-c')
        command += [ '-o', outputPath, '-D', debugDefine, sourcePath ]
        if link:
            command += [os.path.join(self.srcPath, "gc.cpp") ]
        return self.runCommand(command)

    def checkError(self, source, message):
        path = self.writeSource(source)
        for debugMode in (True, False):
            rc, output = self.compileSource(path, path + ".out", debugMode, False)
            self.assertNotEqual(0, rc,
                                "Compilation succeeded, expected error: " + message)
            self.assertIn(message, output,
                          "Compilation failed but expected error: " + message)
        os.remove(path)

    def checkOk(self, source):
        path = self.writeSource(source)
        outputPath = path + ".out"
        for debugMode in (True, False):
            rc, output = self.compileSource(path, outputPath, debugMode, True)
            self.assertEqual(0, rc, "Compilation failed:\n " + output)
            rc, output = self.runCommand([outputPath])
            self.assertEqual(0, rc, "Execution failed:\n " + output)
            self.assertEqual("", output)
            os.remove(outputPath)
        os.remove(path)

    def testAPI(self):
        # Check we can't pass a raw pointer to an API expecting a traced pointer
        self.checkError(
            """
            void api(Traced<Cell*> arg) {}
            void test() {
                Cell* cell;
                api(cell);
            }
            """,
            "no known conversion from 'Cell *' to 'Traced<Cell *>'")
        # Check we can pass a Root to an API expecting a traced pionter
        self.checkOk(
            """
            void api(Traced<Cell*> arg) {}
            void test() {
                Root<Cell*> root;
                api(root);
            }
            """);

unittest.main()