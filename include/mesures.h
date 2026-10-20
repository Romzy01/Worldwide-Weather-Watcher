/**
 * @file    mesures.h
 * @brief   Relevé de tous les capteurs actifs et horodatage.
 *            BME680 : pression, température, hygrométrie.
 *            Light  : luminosité ramenée sur 0-1023.
 *            GPS    : position (trame GGA).   RTC : date et heure.
 *
 *          L'acquisition est non bloquante : mesures_demarrer() lance un
 *          relevé, mesures_poursuivre() est appelée à chaque tour de boucle
 *          jusqu'à ACQ_TERMINEE. Un capteur qui ne répond pas est réessayé
 *          jusqu'à TIMEOUT, puis sa valeur devient NA ; après 2 relevés
 *          consécutifs en timeout, il est signalé en erreur.
 */
#ifndef MESURES_H
#define MESURES_H

#include "types_station.h"

/** Initialise les capteurs (ADC, RTC, BME680, GPS). */
void mesures_init(void);

/**
 * Lance un relevé.
 * @param avec_gps false en mode économie une mesure sur deux (GPS noté NA)
 */
void mesures_demarrer(bool avec_gps);

/**
 * Fait avancer le relevé en cours sans bloquer.
 * @param m structure remplie quand le relevé est terminé
 * @return ACQ_TERMINEE quand toutes les valeurs (ou NA) sont connues
 */
EtatAcquisition mesures_poursuivre(Mesures *m);

/** Abandonne un relevé en cours (changement de mode). */
void mesures_annuler(void);

/** true si un relevé est en cours. */
bool mesures_en_cours(void);

/** Compare les valeurs aux seuils MIN/MAX de la configuration. */
bool mesures_verifier_coherence(const Mesures *m);

/** Erreur la plus prioritaire détectée pendant le dernier relevé. */
CodeErreur mesures_derniere_erreur(void);

/** Lecture directe de la RTC (utilisée aussi par FatFs pour dater les fichiers). */
bool mesures_horodater(RTC_DateTime *dt);

#endif
