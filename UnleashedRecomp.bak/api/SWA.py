import os

srcDirectoryPath = os.path.dirname(os.path.realpath(__file__))

def generateIncludes(f, x):
    for (directoryPath, dirNames, fileNames) in os.walk(srcDirectoryPath + "/" + x):
        prefix = directoryPath[directoryPath.index(x):].replace("\\", "/")

        if prefix.startswith("SWA/SWA"):
            prefix = prefix[4:]
    
        if "detail" in prefix:
            continue
    
        for fileName in fileNames:
            if fileName.endswith(".h"):
                f.write("#include <{}/{}>\n".format(prefix, fileName))

    f.write("\n")
    
with open(srcDirectoryPath + "/SWA.h", "w") as f:
    f.write("#pragma once\n\n")

    generateIncludes(f, "Hedgehog")
    generateIncludes(f, "CSD")
    generateIncludes(f, "SWA")