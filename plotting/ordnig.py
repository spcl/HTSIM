import os

sets = [{"topology" : "BCube", "routing" : ["Minimal"], "siz_s" : [16, 36], "siz_l": [81, 1024]},
{"topology" : "Dragonfly", "routing" : ["Minimal", "Valiants"], "siz_s" : [21, 42], "siz_l": [90, 900]},
{"topology" : "Slimfly", "routing" : ["Minimal", "Valiants"], "siz_s" : [18, 36], "siz_l": [100, 968]},
{"topology" : "Hammingmesh", "routing" : ["Minimal"], "siz_s" : [16, 36], "siz_l": [81, 980]},
{"topology" : "Fat_Tree", "routing" : ["Minimal"], "siz_s" : [16, 32], "siz_l": [128, 1024]}]

incast_size = [64, 128]
tp = ["All-reduce", "All-to-all", "Incast", "Permutation"]
ms = [1, 8]

files = ["cdf.pdf", "cdf.png", "cdf.svg", "completion.pdf", "completion.png", "completion.svg", "nack.pdf", "nack.png", "nack.svg", "outBBR.txt", "outNDP.txt", "outSMaRTT.txt", "violin_fct.pdf", "violin_fct.png", "violin_fct.svg"]

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
                strn = ""
                if(tra == "Incast"):
                    for i,s in enumerate(siz):
                        strn = "corvin{}{}{}{}_{}N{}MiB".format(top, rt, tra, incast_size[i], s, mes)
                        # for file in files:
                            # os.system("mkdir --parents ./experiments/corvin/{}/{}/{}/{}N/{}MiB/ && mv ./experiments/{}/{} ./experiments/corvin/{}/{}/{}/{}N/{}MiB/".format(top, rt, tra, s, mes, strn, file, top, rt, tra, s, mes))
                        os.system("rmdir ./experiments/{}".format(strn))
                else:
                    for s in siz:
                        strn = "corvin{}{}{}{}N{}MiB".format(top, rt, tra, s, mes)
                        # for file in files:
                            # os.system("mkdir --parents ./experiments/corvin/{}/{}/{}/{}N/{}MiB/ && mv ./experiments/{}/{} ./experiments/corvin/{}/{}/{}/{}N/{}MiB/".format(top, rt, tra, s, mes, strn, file, top, rt, tra, s, mes))
                        os.system("rmdir ./experiments/{}".format(strn))