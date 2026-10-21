// Decompiles every function of the program into <dir>/<program>.c, each
// preceded by its address and name.
import ghidra.app.decompiler.*;
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.io.*;

public class DecompileAll extends GhidraScript {
	@Override
	public void run() throws Exception {
		String dir = getScriptArgs()[0];
		DecompInterface decompiler = new DecompInterface();
		decompiler.openProgram(currentProgram);
		int done = 0, failed = 0;
		try (PrintWriter out = new PrintWriter(new FileWriter(dir + "/" + currentProgram.getName() + ".c"))) {
			for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
				if (f.isThunk() || f.isExternal()) continue;
				DecompileResults r = decompiler.decompileFunction(f, 60, monitor);
				out.println("// ==== " + f.getEntryPoint() + " " + f.getName(true));
				if (r != null && r.decompileCompleted()) out.println(r.getDecompiledFunction().getC());
				else { out.println("// decompilation failed"); ++failed; }
				if (++done % 1000 == 0) println(done + " functions");
			}
		}
		println("decompiled " + done + ", failed " + failed);
	}
}
