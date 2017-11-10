import re
import pandas as pd
import matplotlib.pyplot as plt
import sys
import os

class Results:
    def __init__(self, compilation_type, benchmark, date):
        self.compilation_type = compilation_type
        self.bench_name = benchmark
        self.date = date
        self.results = []
        self.pandas_results = None

    def pandasfy(self):
        basic_metrics = []
        values_basic_metrics = []
        extended_metrics = []
        values_extended_metrics = []
        var_metrics = []
        var_extended_metrics = []
        for result in self.results:
            #print result
            basic_metric_result = result[0].replace(',', '')
            if len(result) == 3:
                # basic metric
                values_basic_metrics.append(int(basic_metric_result))
                basic_metrics.append(result[1])
                var_metrics.append(result[2])
            elif len(result) == 4:
                # extended metric
                values_basic_metrics.append(int(basic_metric_result))
                basic_metrics.append(result[1])
                values_extended_metrics.append(float(result[2].split(' ')[0].replace('%', '')))
                extended_metrics.append(re.split(r"\d+\.\d*", result[2])[1].lstrip())
                var_metrics.append(result[3])

        all_metrics = basic_metrics + extended_metrics
        all_values_metrics = values_basic_metrics + values_extended_metrics

        pandas_results = pd.DataFrame(all_values_metrics, index=all_metrics, columns=[self.compilation_type])
        return pandas_results


    def stringfy_result(self, result):
        is_short = False
        if len(result) == 3:
            is_short = True

        rep = ""

        for i in result:
            if is_short:
                if i == "cpu-cycles:u":
                    rep += i + "						"
                elif i == "cache-references:u":
                    rep += i + "					"
                elif i == "branch-instructions:u":
                    rep += i + "					"
                else:
                    rep += i + "\t\t"
            elif i.endswith("refs"):
                rep += i + "\t"
            elif i.endswith("elapsed"):
                rep += i + "\t"
            else:
                rep += i + "\t\t"
        return rep.rstrip()

    def __repr__(self):
        rep = self.compilation_type + '\n'
        rep += self.date + '\n'
        rep += self.bench_name + '\n'
        for res in self.results:
            if res[0].endswith("elapsed"):
                rep += '\n\t'
            rep += '\t' + self.stringfy_result(res) + '\n'
        return rep



def main():

    folder_csv = sys.argv[1]
    folder_xls = sys.argv[2]
    folder_with_perf_files = sys.argv[3]


    all_dfs = []
    for filename in sorted(os.listdir(folder_with_perf_files)):
        order = ""

        # this variable contains all the results from a benchmark type
        # each item of the list is an Results object
        # each Results object contains a list of the results
        all_results = []

        # Reading file
        with open(folder_with_perf_files + '/' + filename, 'rb') as perf_file:
            for line in perf_file:
                if line.startswith("#") and not line.startswith("# started"):
                    result = line.rstrip()
                    date = perf_file.next().rstrip()
                    order += result
                    all_results.append(Results(result[1:], perf_file.name, date))
        perf_file.close()

        all_compilations = filter(None, order.split("#"))
        i = -1
        with open(folder_with_perf_files + '/' + filename, 'rb') as perf_file:
            for line in perf_file:
                if not any(x in line for x in all_compilations):
                    if not line.startswith("#") and not line.startswith(" Performance") and not line.startswith("\n"):
                        all_results[i].results.append(filter(lambda x: x != "#", filter(None, re.split(r'\s{3,}', line.rstrip()))))
                else:
                    i += 1
        perf_file.close()


        # Transforming results in a Pandas Dataframe to easier access to plot graphs
        frames = []
        for each in all_results:
            frames.append(each.pandasfy())

        df = pd.concat(frames, axis=1).transpose()
        df.columns.name = filename.split('.')[0]

        # debug printing
        pd.set_option('expand_frame_repr', False)
        #print df

        baseline = df.loc['gcc-c-minilua-O3']
        gcc_mini_o2 = df.loc['gcc-c-minilua-O2']
        gcc_base_o2 = df.loc['gcc-hybrid-base-O2']
        gcc_base_o3 = df.loc['gcc-hybrid-base-O3']
        gcc_base_pre_o2 = df.loc['gcc-hybrid-base-prefetch-O2']
        gcc_base_pre_o3 = df.loc['gcc-hybrid-base-prefetch-O3']
        clang_mini_o2 = df.loc['clang-c-minilua-O2']
        clang_mini_o3 = df.loc['clang-c-minilua-O3']
        clang_base_o2 = df.loc['clang-hybrid-base-O2']
        clang_base_o3 = df.loc['clang-hybrid-base-O3']
        clang_base_pre_o2 = df.loc['clang-hybrid-base-prefetch-O2']
        clang_base_pre_o3 = df.loc['clang-hybrid-base-prefetch-O3']

        inst = 'instructions:u'
        inst_per_cycle = 'insn per cycle'
        perc_cache_refs = '% of all cache refs'
        branch_inst = 'branch-instructions:u'
        perc_all_branch_misses = '% of all branches' #(missed)

        metrics = [inst, inst_per_cycle, perc_cache_refs, branch_inst, perc_all_branch_misses]

        gcc_c_minilua_O2 = 'gcc-c-minilua-O2'
        gcc_hybrid_base_O2 = 'gcc-hybrid-base-O2'
        gcc_hybrid_base_prefetch_O2 = 'gcc-hybrid-base-prefetch-O2'
        gcc_c_minilua_O3 = 'gcc-c-minilua-O3'
        gcc_hybrid_base_O3 = 'gcc-hybrid-base-O3'
        gcc_hybrid_base_prefetch_O3 = 'gcc-hybrid-base-prefetch-O3'
        clang_c_minilua_O2 = 'clang-c-minilua-O2'
        clang_hybrid_base_O2 = 'clang-hybrid-base-O2'
        clang_hybrid_base_prefetch_O2 = 'clang-hybrid-base-prefetch-O2'
        clang_c_minilua_O3 = 'clang-c-minilua-O3'
        clang_hybrid_base_O3 = 'clang-hybrid-base-O3'
        clang_hybrid_base_prefetch_O3 = 'clang-hybrid-base-prefetch-O3'

        versions = [gcc_c_minilua_O2, gcc_hybrid_base_O2, gcc_hybrid_base_prefetch_O2, gcc_hybrid_base_O3, gcc_hybrid_base_prefetch_O3, clang_c_minilua_O2, clang_hybrid_base_O2, clang_hybrid_base_prefetch_O2, clang_c_minilua_O3, clang_hybrid_base_O3, clang_hybrid_base_prefetch_O3]


    # list of metrics for ploting:
    #   instructions:u
    #   insn per cycle
    #   % of all cache refs
    #   branch-instructions:u
    #   % of all branches (missed)

        # modify df to normalize on gcc c-minilua -O3
        for metric in df.columns:
            for version in versions:
                if version != 'gcc-c-minilua-O3':
                    df.loc[version][metric] = df.loc[version][metric] / df.loc['gcc-c-minilua-O3'][metric]

        for metric in df.columns:
            df.loc['gcc-c-minilua-O3'][metric] = 1.0

        new_df = df.ix[versions, metrics]
        # 'gcc_c_minilua_O3':	'Baseline'
        # 'gcc_hybrid_base_O3': 'V1'
        # 'gcc_hybrid_base_prefetch_O3: 'V2'
        # 'gcc_c_minilua_O2': 'V3'
        # 'gcc_hybrid_base_O2': 'V4'
        # 'gcc_hybrid_base_prefetch_O2': 'V5'
        # 'clang_c_minilua_O3': 'V6'
        # 'clang_hybrid_base_O3':	'V7'
        # 'clang_hybrid_base_prefetch_O3': 'V8'
        # 'clang_c_minilua_O2': 'V9'
        # 'clang_hybrid_base_O2': 'V10'
        # 'clang_hybrid_base_prefetch_O2': 'V11'

        #print new_df.index
        # new_df.rename(index={u'gcc-hybrid-base-O3': 'V1',
        #                      u'gcc-hybrid-base-prefetch-O3':
        #                      u'V2', 'gcc-c-minilua-O2': 'V3',
        #                      u'gcc-hybrid-base-O2': 'V4',
        #                      u'gcc-hybrid-base-prefetch-O2': 'V5',
        #                      u'clang-c-minilua-O3': 'V6',
        #                      u'clang-hybrid-base-O3': 'V7',
        #                      u'clang-hybrid-base-prefetch-O3': 'V8',
        #                      u'clang-c-minilua-O2': 'V9',
        #                      u'clang-hybrid-base-O2': 'V10',
        #                      u'clang-hybrid-base-prefetch-O2': 'V11'})
        new_df.to_csv(folder_csv + filename + '.csv', sep=',')
        all_dfs.append(new_df)



    writer = pd.ExcelWriter(folder_xls + '/' + 'output.xlsx')
    for d in all_dfs:
        d.to_excel(writer, d.columns.name)

    writer.save()



if __name__ == "__main__":
    main()
