
import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("foo.csv")
mcs = df['MCS'].unique()
mcs

for m in mcs:
    df_mcs_1 = df[df['MCS'] == m][['Is_line_of_sight','MCS','Distance_from_tx','Packet_loss']]
    df_mcs_1['Packet_reception_rate'] = 100 - df_mcs_1['Packet_loss']
    df_mcs_1_drop = df_mcs_1.drop_duplicates()
    
    df_LOS = df_mcs_1_drop[df_mcs_1_drop['Is_line_of_sight'] == 'yes']
    df_NLOS = df_mcs_1_drop[df_mcs_1_drop['Is_line_of_sight'] == 'no']
    
    fig = plt.figure(figsize=(12, 6), dpi=300)
    ax = fig.add_subplot(1, 1, 1)
    ax.set_xlabel('Distance between TX and RX [cm]', fontsize=14)
    ax.set_ylabel('Packet Reception Rate [%]', fontsize=14)
    ax.set_xlim([200,3700])
    ax.set_ylim([-5,105])
    ax.set_xticks([400,800,1200,1600,2000,2400,2800,3200,3600])
    ax.set_yticks([0,20,40,60,80,100])

    ax.scatter(x=df_LOS['Distance_from_tx']/10.0, y=df_LOS['Packet_reception_rate'], alpha=0.8, c='green',label="LOS")
    ax.scatter(x=df_NLOS['Distance_from_tx']/10.0, y=df_NLOS['Packet_reception_rate'], alpha=0.8, c='red',label="NLOS")

    legend = ax.legend(loc='upper left', bbox_to_anchor=(1,1), fontsize=14)

    plt.tight_layout(pad=3)
    plt.title(m, fontsize=16)
    plt.savefig("export_MCS_"+ m + ".png")
