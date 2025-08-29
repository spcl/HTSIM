# extract_max_qs.awk
/compqueue/ {
    match($0, /compqueue\([0-9]+Mb\/s,[0-9]+bytes\)([^ ]*)/, arr);
    name = arr[1];
    qs = $NF;
    tr = $(NF-1);
    if (qs > max_qs) {
        max_qs = qs;
        max_name = name;
        max_tr = tr;
    }
}
END {
    filename = FILENAME; 
    gsub(".*/", "", filename);
    print "Queue max: " max_qs ", Trimming ratio: " max_tr ", At: " max_name ", File: ", filename;
}
