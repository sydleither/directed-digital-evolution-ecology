import sys
import numpy as np
import pandas as pd
import seaborn as sns
import matplotlib.pyplot as plt


def format_input_string(input_string):
    str_to_list = input_string[1:-2].split(' ')
    vals_to_int = [int(x) for x in str_to_list]
    return vals_to_int


def main(file_location):
    df = pd.read_csv(f'{file_location}/offspring_count.csv')
    df['species'] = df['species'].apply(format_input_string)
    df['offspring'] = df['offspring'].apply(format_input_string)
    df = df.explode(['species', 'offspring'])
    
    #assuming only one epoch recorded
    for world in df['world'].unique():
        df_world = df.loc[df['world'] == world]
        fig, ax = plt.subplots(1, 1, tight_layout=True)
        ax = sns.boxplot(data=df_world, x='species', y='offspring')
        plt.title(f'World {world} offspring count variability')
        plt.savefig(f'offspring_counts_{world}.png')
        plt.close()

    for world in df['world'].unique():
        variances = []
        df_world = df.loc[df['world'] == world]
        for species in df_world['species'].unique():
            df_world_species = df_world.loc[df_world['species'] == species]
            species_offspring = df_world_species['offspring'].tolist()
            if len(species_offspring) != 10:
                print(species_offspring)
            variances.append(np.var(species_offspring))
        #print(variances)


if __name__ == '__main__':
    main(sys.argv[1])