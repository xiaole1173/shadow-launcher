import java.io.*;
import java.nio.file.*;
import net.minecraftforge.srgutils.IMappingFile;

/**
 * Converts ProGuard obfuscation mapping (.txt) to TSRG format (.tsrg)
 * using the exact same srgutils library that FART uses.
 * 
 * Embedded in ShadowLauncher as a pre-compiled .class resource.
 * No javac needed at runtime -- only java (JRE).
 * 
 * Usage: java -cp srgutils.jar;. ProGuardToTSRG <input.txt> <output.tsrg>
 */
public class ProGuardToTSRG {
    public static void main(String[] args) throws Exception {
        if (args.length < 2) {
            System.err.println("Usage: java -cp srgutils.jar;. ProGuardToTSRG <in.txt> <out.tsrg>");
            System.exit(1);
        }
        File inFile = new File(args[0]);
        File outFile = new File(args[1]);
        outFile.getParentFile().mkdirs();
        
        try (InputStream is = new BufferedInputStream(new FileInputStream(inFile))) {
            IMappingFile mapping = IMappingFile.load(is);
            if (mapping == null) {
                System.err.println("Failed to load ProGuard mapping from: " + args[0]);
                System.exit(1);
            }
            mapping.write(outFile.toPath(), IMappingFile.Format.TSRG, false);
        }
        
        System.out.println("TSRG written: " + outFile.getAbsolutePath() 
            + " (" + outFile.length() + " bytes)");
    }
}
