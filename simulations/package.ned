package flora_tdma.simulations;

import inet.applications.udpapp.UdpBasicApp;
import inet.node.inet.Router;
import inet.node.internetcloud.InternetCloud;
import flora_tdma.LoRaPhy.LoRaMedium;
import flora_tdma.LoraNode.LoRaNode;
import flora_tdma.LoraNode.LoRaGW;
import inet.node.inet.StandardHost;
import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.ethernet.Eth1G;

@license(LGPL);
network LoRaNetworkTest
{
    parameters:
        int numberOfNodes = default(1);
        int numberOfGateways = default(1);
        int networkSizeX = default(200);
        int networkSizeY = default(200);
        @display("bgb=200,200");
    submodules:
        loRaNodes[numberOfNodes]: LoRaNode {
            @display("p=50,50");
        }
        loRaGW[numberOfGateways]: LoRaGW {
            @display("p=100,100;is=s");
        }
        LoRaMedium: LoRaMedium {
            @display("p=180,180");
        }
    connections:
}

