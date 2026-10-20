/**
 * @file    modes.c
 * @brief   Machine à états des 4 modes (voir modes.h).
 *
 *  Transitions retenues (identiques aux diagrammes du livrable 1) :
 *    Démarrage      + bouton rouge enfoncé -> CONFIGURATION
 *    Démarrage      + aucun bouton         -> STANDARD
 *    CONFIGURATION  + 30 min sans commande -> STANDARD   (ou commande EXIT)
 *    CONFIGURATION  + appui bouton         -> ignoré
 *    STANDARD       + vert maintenu 5 s    -> ECONOMIE
 *    STANDARD       + rouge maintenu 5 s   -> MAINTENANCE
 *    ECONOMIE       + rouge maintenu 5 s   -> STANDARD   (ECONOMIE -> MAINTENANCE interdit)
 *    MAINTENANCE    + rouge maintenu 5 s   -> mode précédent
 */
#include "modes.h"
#include "boutons.h"
#include "led_etat.h"
#include "mesures.h"
#include "journal_sd.h"
#include "config.h"
#include "console.h"
#include "temps.h"
#include <stdio.h>

static Mode     mode_courant = MODE_STANDARD;
static Mode     mode_precedent = MODE_STANDARD;
static Minuteur minuteur_mesure;
static Minuteur minuteur_inactivite;
static bool     gps_a_relever = true;
static Mesures  derniere_mesure;
static char     commande[TAILLE_COMMANDE_MAX];

static const char *const NOMS_MODES[] = {
    [MODE_STANDARD] = "STANDARD", [MODE_CONFIGURATION] = "CONFIGURATION",
    [MODE_MAINTENANCE] = "MAINTENANCE", [MODE_ECONOMIE] = "ECONOMIE",
};

static void afficher_aide_configuration(void)
{
    printf("Commandes : PARAMETRE=valeur (ex. LOG_INTERVAL=5), RESET, VERSION, PARAMS,\r\n");
    printf("            CLOCK=HH:MM:SS, DATE=MM,JJ,AAAA, DAY=MON..SUN, EXIT\r\n");
    printf("Retour automatique en mode standard apres %lu min sans commande.\r\n",
           (unsigned long)INACTIVITE_CONFIG_MIN);
}

/* Actions réalisées en quittant un mode. */
static void sortir(Mode ancien)
{
    mesures_annuler();                       /* un relevé en cours est abandonné */
    if (ancien == MODE_CONFIGURATION) {
        minuteur_arreter(&minuteur_inactivite);
        if (!config_enregistrer_si_modifie())
            console_repondre("ERREUR : parametres non sauvegardes en flash");
    }
    if (ancien == MODE_MAINTENANCE) journal_reprendre();
}

/* Actions réalisées en entrant dans un mode. */
static void entrer(Mode nouveau)
{
    led_afficher_mode(nouveau);
    led_signaler_erreur(ERR_AUCUNE);
    printf("\r\n=== Mode %s ===\r\n", NOMS_MODES[nouveau]);

    switch (nouveau) {
    case MODE_STANDARD:
    case MODE_ECONOMIE:
        if (!journal_disponible()) journal_init();
        gps_a_relever = true;
        minuteur_demarrer(&minuteur_mesure, 0);   /* premier relevé immédiat */
        break;
    case MODE_CONFIGURATION:
        console_vider();
        afficher_aide_configuration();
        minuteur_demarrer(&minuteur_inactivite, INACTIVITE_CONFIG_MIN * temps_minute_ms());
        break;
    case MODE_MAINTENANCE:
        journal_suspendre();                      /* la carte SD peut être retirée */
        minuteur_demarrer(&minuteur_mesure, 0);
        break;
    }
}

void modes_init(void)
{
    mode_courant = boutons_rouge_presse() ? MODE_CONFIGURATION : MODE_STANDARD;
    mode_precedent = MODE_STANDARD;
    entrer(mode_courant);
}

void modes_changer(Mode nouveau)
{
    if (nouveau == mode_courant) return;
    sortir(mode_courant);
    mode_precedent = mode_courant;
    mode_courant = nouveau;
    entrer(nouveau);
}

Mode modes_courant(void)
{
    return mode_courant;
}

void modes_traiter_evenement(EvenementBouton e)
{
    if (e == BOUTON_AUCUN) return;

    switch (mode_courant) {
    case MODE_STANDARD:
        if (e == BOUTON_VERT_LONG)  modes_changer(MODE_ECONOMIE);
        if (e == BOUTON_ROUGE_LONG) modes_changer(MODE_MAINTENANCE);
        break;
    case MODE_ECONOMIE:
        if (e == BOUTON_ROUGE_LONG) modes_changer(MODE_STANDARD);
        break;
    case MODE_MAINTENANCE:
        if (e == BOUTON_ROUGE_LONG) modes_changer(mode_precedent);
        break;
    case MODE_CONFIGURATION:
        break;                     /* boutons ignorés en configuration */
    }
}

/* Hors configuration, les caractères reçus sont jetés avec un rappel. */
static void ignorer_console(void)
{
    if (console_vider())
        console_repondre("(commandes acceptees uniquement en mode configuration)");
}

/*
 * Relevé périodique commun aux modes standard, économie et maintenance.
 * @param intervalle_ms durée entre deux débuts de relevé
 * @param economie      true : GPS relevé une mesure sur deux
 * @param enregistrer   true : écriture SD ; false : affichage seul (maintenance)
 */
static void cycle_releve(uint32_t intervalle_ms, bool economie, bool enregistrer)
{
    if (!mesures_en_cours()) {
        if (!minuteur_expire(&minuteur_mesure)) return;
        bool avec_gps = true;
        if (economie) {                      /* GPS une mesure sur deux */
            avec_gps = gps_a_relever;
            gps_a_relever = !gps_a_relever;
        }
        mesures_demarrer(avec_gps);
        minuteur_demarrer(&minuteur_mesure, intervalle_ms);
    }

    if (mesures_poursuivre(&derniere_mesure) == ACQ_EN_COURS) return;

    console_afficher_mesures(&derniere_mesure);
    CodeErreur erreur = mesures_derniere_erreur();
    if (enregistrer) {
        CodeErreur erreur_sd = journal_ecrire(&derniere_mesure);
        if (erreur == ERR_AUCUNE) erreur = erreur_sd;
    }
    if (erreur != ERR_AUCUNE) printf("[ERREUR] %s\r\n", led_nom_erreur(erreur));
    led_signaler_erreur(erreur);
}

void mode_standard(void)
{
    ignorer_console();
    cycle_releve((uint32_t)params.log_interval_min * temps_minute_ms(), false, true);
}

void mode_economie(void)
{
    ignorer_console();
    cycle_releve(2UL * (uint32_t)params.log_interval_min * temps_minute_ms(), true, true);
}

void mode_maintenance(void)
{
    ignorer_console();
    cycle_releve(PERIODE_MAINTENANCE_MS, false, false);   /* rien n'est écrit sur la SD */
}

void mode_configuration(void)
{
    if (console_lire_commande(commande, sizeof(commande))) {
        minuteur_rearmer(&minuteur_inactivite);
        switch (config_traiter_commande(commande)) {
        case CMD_OK:             console_repondre("OK"); break;
        case CMD_DEJA_REPONDU:   break;
        case CMD_INCONNUE:       console_repondre("ERREUR : commande inconnue"); break;
        case CMD_HORS_DOMAINE:   console_repondre("ERREUR : valeur hors domaine"); break;
        case CMD_ECHEC_MATERIEL: console_repondre("ERREUR : RTC ou flash ne repond pas"); break;
        case CMD_SORTIE:
            console_repondre("OK");
            modes_changer(MODE_STANDARD);
            return;
        }
    }
    if (minuteur_expire(&minuteur_inactivite)) {
        console_repondre("Inactivite : fin de la configuration.");
        modes_changer(MODE_STANDARD);
    }
}

void modes_executer(void)
{
    switch (mode_courant) {
    case MODE_STANDARD:      mode_standard(); break;
    case MODE_ECONOMIE:      mode_economie(); break;
    case MODE_CONFIGURATION: mode_configuration(); break;
    case MODE_MAINTENANCE:   mode_maintenance(); break;
    }
}
