import os
filename = "Appendix.txt"
f = open(filename, "w")

sets = [{"topology" : "BCube", "routing" : ["Minimal"], "siz_s" : [16, 36], "siz_l": [81, 1024]},
{"topology" : "Dragonfly", "routing" : ["Minimal", "Valiants"], "siz_s" : [21, 42], "siz_l": [90, 900]},
{"topology" : "Slimfly", "routing" : ["Minimal", "Valiants"], "siz_s" : [18, 36], "siz_l": [100, 968]},
{"topology" : "Hammingmesh", "routing" : ["Minimal"], "siz_s" : [16, 36], "siz_l": [81, 980]},
{"topology" : "Fat_Tree", "routing" : ["Minimal"], "siz_s" : [16, 32], "siz_l": [128, 1024]}]

incast_size = [64, 128]
tp = ["All-reduce", "All-to-all", "Incast", "Permutation"]
ms = [1, 8]

files = ["cdf.png", "completion.png", "nack.png", "violin_fct.png"]

for set in sets:
    top = set["topology"]
    rou = set["routing"]
    sizs = set["siz_s"]
    sizl = set["siz_l"]
    for rt in rou:
        for tra in tp:
            siz = []
            if (tra == "All-reduce" or tra == "All-to-all"):
                siz = sizs
            else:
                siz = sizl
            for mes in ms:
                for s in siz:
                    print("\\begin{figure}[ht]", file=f)
                    print("\t\\begin{subfigure}[b]{0.4\\textwidth}", file=f)
                    print("\t\t\\includegraphics[width=\\textwidth]{" + "Figures/Plots/{}/{}/{}/{}N/{}MiB/cdf.png".format(top, rou, tra, s, mes) + "}", file=f)
                    print("\t\t\\label{" + "plt:{}/{}/{}/{}N/{}MiB/cdf".format(top, rou, tra, s, mes) + "}", file=f)
                    print("\t\\end{subfigure}", file=f)
                    print("\t\\hfill", file=f)
                    print("\t\\begin{subfigure}[b]{0.4\\textwidth}", file=f)
                    print("\t\t\\includegraphics[width=\textwidth]{" + "Figures/Plots/{}/{}/{}/{}N/{}MiB/violin_fct.png".format(top, rou, tra, s, mes) + "}", file=f)
                    print("\t\t\\label{" + "plt:{}/{}/{}/{}N/{}MiB/violin_fct".format(top, rou, tra, s, mes) + "}", file=f)
                    print("\t\\end{subfigure}", file=f)
                    print("\t\\hfill", file=f)
                    print("\t\\begin{subfigure}[b]{0.4\\textwidth}", file=f)
                    print("\t\t\\includegraphics[width=\\textwidth]{" + "Figures/Plots/{}/{}/{}/{}N/{}MiB/completion.png".format(top, rou, tra, s, mes) + "}", file=f)
                    print("\t\t\\label{" + "plt:{}/{}/{}/{}N/{}MiB/completion".format(top, rou, tra, s, mes) + "}", file=f)
                    print("\t\\end{subfigure}", file=f)
                    print("\t\\hfill", file=f)
                    print("\t\\begin{subfigure}[b]{0.4\\textwidth}", file=f)
                    print("\t\t\\includegraphics[width=\\textwidth]{" + "Figures/Plots/{}/{}/{}/{}N/{}MiB/nack.png".format(top, rou, tra, s, mes) + "}", file=f)
                    print("\t\t\\label{" + "plt:{}/{}/{}/{}N/{}MiB/nack".format(top, rou, tra, s, mes) + "}", file=f)
                    print("\t\\end{subfigure}", file=f)
                    print("\t\\caption{" + "{}/{}/{}/{}N/{}MiB".format(top, rou, tra, s, mes) + "}", file=f)
                    print("\\end{figure}", file=f)