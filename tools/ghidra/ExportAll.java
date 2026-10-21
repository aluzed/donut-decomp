// Exports a program's functions, strings (with the functions referring to them),
// imports and class namespaces as tab-separated files into the directory given
// as the script's argument.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.address.*;
import java.io.*;

public class ExportAll extends GhidraScript {
	@Override
	public void run() throws Exception {
		String dir = getScriptArgs()[0];
		String base = currentProgram.getName();
		try (PrintWriter out = new PrintWriter(new FileWriter(dir + "/" + base + ".functions.tsv"))) {
			for (Function f : currentProgram.getFunctionManager().getFunctions(true))
				out.println(f.getEntryPoint() + "\t" + f.getBody().getNumAddresses() + "\t" + f.getName(true)
				            + "\t" + (f.isThunk() ? "thunk" : "") + "\t" + f.getCallingConventionName());
		}
		ReferenceManager refs = currentProgram.getReferenceManager();
		FunctionManager fm = currentProgram.getFunctionManager();
		try (PrintWriter out = new PrintWriter(new FileWriter(dir + "/" + base + ".strings.tsv"))) {
			for (Data d : currentProgram.getListing().getDefinedData(true)) {
				if (!d.hasStringValue()) continue;
				StringBuilder users = new StringBuilder();
				for (Reference r : refs.getReferencesTo(d.getAddress())) {
					Function f = fm.getFunctionContaining(r.getFromAddress());
					users.append(f == null ? r.getFromAddress().toString() : f.getEntryPoint() + ":" + f.getName(true)).append(' ');
				}
				String v = String.valueOf(d.getValue()).replace("\t", "\\t").replace("\n", "\\n").replace("\r", "\\r");
				out.println(d.getAddress() + "\t" + v + "\t" + users.toString().trim());
			}
		}
		try (PrintWriter out = new PrintWriter(new FileWriter(dir + "/" + base + ".imports.tsv"))) {
			for (Symbol s : currentProgram.getSymbolTable().getExternalSymbols())
				out.println(s.getParentNamespace().getName() + "\t" + s.getName() + "\t"
				            + refs.getReferenceCountTo(s.getAddress()));
		}
		try (PrintWriter out = new PrintWriter(new FileWriter(dir + "/" + base + ".exports.tsv"))) {
			for (Symbol s : currentProgram.getSymbolTable().getAllSymbols(true))
				if (s.isExternalEntryPoint())
					out.println(s.getAddress() + "\t" + s.getName(true));
		}
		try (PrintWriter out = new PrintWriter(new FileWriter(dir + "/" + base + ".classes.tsv"))) {
			SymbolIterator it = currentProgram.getSymbolTable().getAllSymbols(true);
			java.util.TreeSet<String> names = new java.util.TreeSet<>();
			for (Symbol s : it) {
				Namespace ns = s.getParentNamespace();
				if (ns instanceof GhidraClass) names.add(ns.getName(true));
				if (s.getSymbolType() == SymbolType.CLASS) names.add(s.getName(true));
			}
			for (String n : names) out.println(n);
		}
		println("exported " + base);
	}
}
