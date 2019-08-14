
import pandas as pd
import matplotlib.pyplot as plt

df = pd.read_csv("foo.csv")
mcs = df['MCS'].unique()

for m in mcs:
    df_mcs = df[df['MCS'] == m][['Is_line_of_sight','MCS','Distance_from_tx','Packet_loss']]
    df_mcs['Packet_reception_rate'] = 100 - df_mcs['Packet_loss']
    df_mcs_drop = df_mcs.drop_duplicates()
    
    df_LOS = df_mcs_drop[df_mcs_drop['Is_line_of_sight'] == 'yes']
    df_NLOS = df_mcs_drop[df_mcs_drop['Is_line_of_sight'] == 'no']
    
    fig = plt.figure(figsize=(15, 6), dpi=240)
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
    plt.savefig("PRR_MCS_"+ m + ".png")

for m in mcs:
    df_mcs = df[df['MCS'] == m][['Is_line_of_sight','MCS','Distance_from_tx','Received_power']]
    df_avg_rssi = df_mcs[['Is_line_of_sight','Distance_from_tx','Received_power']].groupby(['Distance_from_tx','Is_line_of_sight'], as_index=False).mean()
    
    df_LOS = df_avg_rssi[df_avg_rssi['Is_line_of_sight'] == 'yes']
    df_NLOS = df_avg_rssi[df_avg_rssi['Is_line_of_sight'] == 'no']
    
    fig = plt.figure(figsize=(15, 6), dpi=240)
    ax = fig.add_subplot(1, 1, 1)
    ax.set_xlabel('Distance between TX and RX [cm]', fontsize=14)
    ax.set_ylabel('Average RSSI [dBm]', fontsize=14)
    ax.set_xlim([200,3700])
    ax.set_ylim([-180,-80])
    ax.set_xticks([400,800,1200,1600,2000,2400,2800,3200,3600])
    ax.set_yticks([-180,-160,-140,-120,-100,-80])
    
    ax.scatter(x=df_LOS['Distance_from_tx']/10.0, y=df_LOS['Received_power'], alpha=0.8, c='green',label="LOS")
    ax.scatter(x=df_NLOS['Distance_from_tx']/10.0, y=df_NLOS['Received_power'], alpha=0.8, c='red',label="NLOS")
    
    legend = ax.legend(loc='upper left', bbox_to_anchor=(1,1), fontsize=14)
    
    plt.tight_layout(pad=3)
    plt.title(m, fontsize=16)
    plt.savefig("RSSI_MCS_"+ m + ".png")
